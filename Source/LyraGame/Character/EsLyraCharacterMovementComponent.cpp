// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/EsLyraCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "LyraCharacter.h"
#include "LyraHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "WorldCollision.h"
#include "AudioParameterControllerInterface.h"
#include "JetpackComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
/**
 *  Get prediction data for a client game
 ****************************************************************************************/
FNetworkPredictionData_Client* UEsLyraCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

	if (ClientPredictionData == nullptr)
	{
		UEsLyraCharacterMovementComponent* MutableThis = const_cast<UEsLyraCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Es(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f; 
	}
	return ClientPredictionData;
}



UEsLyraCharacterMovementComponent::UEsLyraCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(EsNetworkMoveDataContainer);
	Safe_bWantsToTeleport = false;
	Safe_bWantsToWallRun = false;
	Safe_bIsRewinding = false;
	Safe_bWantsToUseJetpack = false;
	Safe_RewindingIndex = 0;
	TeleportStartTime = 0.f;
	RewindTimeEndTime = 0.f;
	
}

void UEsLyraCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ESCharacterOwner = Cast<ALyraCharacter>(GetOwner());
	check(ESCharacterOwner);
	
	LyraHealthComponent = ESCharacterOwner->GetLyraHealthComponent();
	check(LyraHealthComponent);
	
	JetpackComponent = ESCharacterOwner->GetJetpackComponent();
	check(JetpackComponent);
}

void UEsLyraCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	BufferSampleMaxSize = RewindTimeWindowDuration / RewindTimeSampleFrequencyTime;
}


void UEsLyraCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

/**
 *  Update the character state in PerformMovement right before doing the actual position change
 **********************************************************************************************************/
void UEsLyraCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	//Teleport
	const bool bAuthProxy = CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled();
	if(Safe_bWantsToTeleport && CanTeleport())
	{
		if(!bAuthProxy || GetWorld()->GetTimeSeconds() - TeleportStartTime > AuthTeleportCooldownDuration)
		{
			PerformTeleport();
			Safe_bWantsToTeleport = false;		
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Error with the client values (Cheating)"));
		}
	}
	
	//Rewind Time
	if(Safe_bIsRewinding)
	{
		if(!bAuthProxy || GetWorld()->GetTimeSeconds() - RewindTimeEndTime > AuthRewindTimeCooldownDuration - RewindingDuration || GetWorld()->GetTimeSeconds() < AuthRewindTimeCooldownDuration)
		{		
			PerformRewindingTime(DeltaSeconds);	
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Error with the client values (Cheating)"));
		}
	}
	else
	{
		CollectRewindData(DeltaSeconds);		
	}
	
	if (Safe_bWantsToUseJetpack)
	{
		if (!bAuthProxy || CanUseJetpack())
		{
			SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_Jetpacking);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Error with the client values (Cheating)"));
		}
	}

	// Wall Run	
	TryWallRun();

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

/**
 *  PhysCustom including Phys functions for each CustomMovementMode
 **********************************************************************************************************/
void UEsLyraCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{

	switch (CustomMovementMode)
	{	
		case CMOVE_WallRun:
			PhysWallRun(deltaTime, Iterations);
			break;
		case CMOVE_Jetpacking:
			PhysJetpacking(deltaTime, Iterations);
			break;
		default:
			UE_LOG(LogTemp,Warning, TEXT("Invalid Movement Mode"));
	}

	Super::PhysCustom(deltaTime, Iterations);
}

bool UEsLyraCharacterMovementComponent::CanUseJetpack() const 
{
	if (!IsValid(JetpackComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("Jetpack component is null"));
		return false;
	}
		
	
	if(JetpackComponent->GetJetpackResource() <= 0.f)
	{
		return false;
	}

	return true;

}


bool UEsLyraCharacterMovementComponent::IsFalling() const
{
	/* is generating me problems because movement mode is set to 0*/
	return Super::IsFalling() || IsCustomMovementMode(ECustomMovementMode::CMOVE_Jetpacking);
}

bool UEsLyraCharacterMovementComponent::CancelJetpackGameplayAbility() const
{
	bool bIsCanceled = false;
	const ALyraCharacter* LyraCharacter = UECasts_Private::DynamicCast<ALyraCharacter*>(CharacterOwner.Get());

	if(!LyraCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Lyra Character is null while trying to cancel jetpacking ability"));
		return bIsCanceled;
	}
	
	TArray<FGameplayAbilitySpec*> ActiveAbilitiesWithSpecifiedTag;
	ULyraAbilitySystemComponent* LyraAsc = LyraCharacter->GetLyraAbilitySystemComponent();

	if(!LyraAsc)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not retrieve game ability system component while trying to cancel jetpack ability"));
		return bIsCanceled;
	}
	
	LyraAsc->GetActivatableGameplayAbilitySpecsByAllMatchingTags(JetpackGameplayActionTag, ActiveAbilitiesWithSpecifiedTag, true);
	for (const auto AbilityWithSpecifiedTag : ActiveAbilitiesWithSpecifiedTag)
	{
		if(AbilityWithSpecifiedTag->IsActive())
		{
			LyraAsc->CancelAbility(AbilityWithSpecifiedTag->Ability);
			bIsCanceled = true;
			// it should be just one so we exit the loop
			break;
		}
	}

	return bIsCanceled;
}


void UEsLyraCharacterMovementComponent::PhysJetpacking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	
	/* Amount of resource in seconds needed to use the jetpack for this round */
	bool bIsJetpackResourceEnough = false;
	if(IsValid(JetpackComponent))
	{
		const float MaxJetpackResource = JetpackComponent->GetMaxJetpackResource();
		const float ResourceNeededForJetpacking = deltaTime / MaxJetpackResource;
		bIsJetpackResourceEnough  = JetpackComponent->GetJetpackResource() >= ResourceNeededForJetpacking  ;
	}
	
	if ((!Safe_bWantsToUseJetpack  || !bIsJetpackResourceEnough ) && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
    	UE_LOG(LogTemp, Warning, TEXT("Trying to cancel jetpack ability"));
		Safe_bWantsToUseJetpack = false;
		if(!CancelJetpackGameplayAbility())
		{
			UE_LOG(LogTemp, Warning, TEXT("Jetpack ability wasn't canceled"));
		}
		SetMovementMode(EMovementMode::MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		
		return;
	}

	Iterations++;
	bJustTeleported = false;
	
	CharacterOwner->LaunchCharacter( FVector(0,0, JetpackVelocity), false , true);
	auto CurrentRotation = CharacterOwner->GetActorRotation().Quaternion();
	FHitResult Hit(1.f);
	FVector Adjusted = Velocity * deltaTime;
	SafeMoveUpdatedComponent(Adjusted, CurrentRotation, true, Hit);


	
}

void UEsLyraCharacterMovementComponent::Server_ChangeJetpackStatus_Implementation(bool bIsJetpackON)
{
	Safe_bWantsToUseJetpack = bIsJetpackON;
}

bool UEsLyraCharacterMovementComponent::Server_ChangeJetpackStatus_Validate(bool bIsJetpackON)
{
	return true;
}

void UEsLyraCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode,
	uint8 PreviousCustomMode)
{
	
	
	/*
	 * current mode 3 falling
	 * current mode 1 standing
	 * current mode 6 flying/jetpacking ?
	 * custom movement mode nothing 0
	 * custom movement mode jetpacking 2
	 **/


	
	FString netPrefix = GetWorld()->IsNetMode(NM_Client) ? "[CLIENT]" : "[SERVER]";

	
	// if(GEngine)
	// {

	// FString DebugMessage = FString::Printf(TEXT("Current Custom Movement Mode: %u Previous Movement Mode %u \n"), CustomMovementMode,  PreviousCustomMode);
	// GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMessage);
	UE_LOG(LogTemp, Warning, TEXT("SVS %s - Current Custom Movement Mode: %u Previous Movement Mode %u \n"), *netPrefix, CustomMovementMode,  PreviousCustomMode);
	UE_LOG(LogTemp, Warning, TEXT("SVS %s - Current Mode: %d , Previous %u \n"), *netPrefix, MovementMode.GetValue(),  PreviousMovementMode);
	// }

	if(JetpackComponent)
	{
		// if(!JetpackComponent->IsSpecialEffectOn() &&
		// 	PrevPrevCustomMovementMode != 1 &&
		// 	PreviousCustomMode != CMOVE_Jetpacking && CustomMovementMode == CMOVE_Jetpacking)
		if(!JetpackComponent->IsSpecialEffectOn() &&
			PrevPrevCustomMovementMode == 0 &&
			PreviousCustomMode == 0 &&
			CustomMovementMode == CMOVE_Jetpacking
			)
		{
			UE_LOG(LogTemp, Warning, TEXT("SVS - Effect active \n"));

			JetpackComponent->SetJetpackEffects(true);
		}  /*  This is a small hack that makes the effect disappear even though sometimes the information about the "wants to use jetpack" variable update is lost.
		   /* While we cannot rely on the stable change of the isJetpacking since
		   /* we switch back and forward between Falling and Jetpacking movement mode */
		else if( JetpackComponent->IsSpecialEffectOn() &&
			     ((PreviousCustomMode != CMOVE_Jetpacking && CustomMovementMode != CMOVE_Jetpacking) &&
					!Safe_bWantsToUseJetpack))
		{
			UE_LOG(LogTemp, Warning, TEXT("Effect off - Current Custom Movement Mode: %u Previous Movement Mode %u \n"), CustomMovementMode,  PreviousCustomMode);
			JetpackComponent->SetJetpackEffects(false);	
		}
	}
	PrevPrevCustomMovementMode = PreviousCustomMode;
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

/**
 *  Event notification when client receives correction data from the server, before applying the data
 **********************************************************************************************************/
void UEsLyraCharacterMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData,
																   float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
																   bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
									  bHasBase, bBaseRelativePosition,
									  ServerMovementMode, ServerGravityDirection);

	UE_LOG(LogTemp, Warning, TEXT("On Client Correction Received"));
}

/**
 *  Unpack compressed flags from a saved move and set state accordingly
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	Safe_bWantsToTeleport = (Flags & FSavedMove_Es::FLAG_Teleport) != 0;
	Safe_bWantsToWallRun = (Flags & FSavedMove_Es::FLAG_WallRun) != 0;
	Safe_bIsRewinding = (Flags & FSavedMove_Es::FLAG_RewindTime) != 0;
	Safe_bWantsToUseJetpack = (Flags & FSavedMove_Es::FLAG_Jetpack) != 0;
	

}

void UEsLyraCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	const FVector& NewAccel)
{
	const FEsNetworkMoveData* CustomNetworkMoveData = static_cast<FEsNetworkMoveData*>(GetCurrentNetworkMoveData());

	Safe_RewindingIndex = CustomNetworkMoveData->Saved_RewindingIndex;

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);	
}

#pragma region Teleport
/**
 *  Player canTeleport while walking or midair
 **********************************************************************************************************/
bool UEsLyraCharacterMovementComponent::CanTeleport() const
{
	return IsWalking() || IsFalling();
}

/**
 *  Teleport the character by TeleportImpulse. Use velocity if you want to avoid sweep
 **********************************************************************************************************/
void UEsLyraCharacterMovementComponent::PerformTeleport()
{	
	TeleportStartTime = GetWorld()->GetTimeSeconds();	

	const FVector ForwardVector = UpdatedComponent->GetForwardVector();
	if (const FLyraCharacterGroundInfo GroundInfo = GetGroundInfo(); GroundInfo.GroundDistance == 0.f)
	{
		FHitResult Hit;
		//If the angle is greater than zero it means the player is on a slope and sweep will prevent teleport from happening
		if (float GroundAngleInDegrees = FMath::Acos(
				FVector::DotProduct(GroundInfo.GroundHitResult.Normal, FVector::UpVector)) * 180.0f / PI;
			GroundAngleInDegrees > 0.f)
		{
			// Projecting the forward vector will adjust the teleport location to a direction parallel to the slope
			const FVector AdjustedTeleportLocation = FVector::VectorPlaneProject(ForwardVector, GroundInfo.GroundHitResult.Normal);

			SafeMoveUpdatedComponent(AdjustedTeleportLocation * TeleportImpulse, UpdatedComponent->GetComponentRotation(), false, Hit, ETeleportType::None);
		}
		else
		{
			SafeMoveUpdatedComponent(ForwardVector * TeleportImpulse, UpdatedComponent->GetComponentRotation(), true, Hit, ETeleportType::None);
		}

	}

	SetMovementMode(MOVE_Falling);
}

#pragma endregion

#pragma region WallRun

/**
 *  Overwrite CanAttemptJump to read Jump press in air to WallJump or LateJump
 ****************************************************************************************/
bool UEsLyraCharacterMovementComponent::CanAttemptJump() const
{
	Safe_bWantsToWallRun = true;
	return Super::CanAttemptJump() || CanWallJump() || bCanLateJump;
}

/**
 *  Overwrite DoJump excluding the wallJump and including LateJump
 ****************************************************************************************/

bool UEsLyraCharacterMovementComponent::DoJump(bool bReplayingMoves)
{	
	if(CanWallJump())
		return false;
	
	if (Super::DoJump(bReplayingMoves))
	{
		bCanLateJump = false;
		return true;
	}
	
	return false;
}



/**
 *  Called on Jump Released, activates WallJump if in WallRun
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::SetJumpEnd()
{	
	Safe_bWantsToWallRun = false;	

	//Wall Jump
	if(CanWallJump() && WallHit.bBlockingHit)
	{			
		SetMovementMode(MOVE_Falling);		
		Velocity = (CharacterOwner->GetActorForwardVector() + WallHit.Normal + FVector::UpVector * 2) * WallJumpOffForce;
#ifdef ENABLE_DEBUG_LINES
		DrawDebugLine(GetWorld(), Start, Start + Velocity, FColor::Silver, true, 1, 0, 4);
#endif
	}
}

/**
 *  Returns angle between OnGoing direction and Player looking direction
 ****************************************************************************************/
float UEsLyraCharacterMovementComponent::GetLookingAtAngle() const
{
	if(!CharacterOwner) return 0.f;
	
	return FMath::RadiansToDegrees(acosf(FVector::DotProduct(Velocity.GetSafeNormal(), CharacterOwner->GetActorForwardVector())));
}

/**
 *  If JumpKey is pressed, search for a wall-runnable wall
 ****************************************************************************************/
bool UEsLyraCharacterMovementComponent::TryWallRun()
{	
	if(!Safe_bWantsToWallRun)
		return false;
	
	if (!IsFalling())
		return false;

	if(CanLateJump())
		return false;

	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector LeftEnd = Start - UpdatedComponent->GetRightVector() * CapsuleRScaled();
	FVector RightEnd = Start + UpdatedComponent->GetRightVector() * CapsuleRScaled();

	FCollisionQueryParams Params = ESCharacterOwner->GetIgnoreCharacterParams();	
	
	FHitResult FloorHit;
	// Check Player Height
	if (GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapsuleHH() + MinWallRunHeight), "BlockAll", Params))
	{		
		return false;
	}
	
	// Left Cast
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, LeftEnd, "BlockAll", Params);

	if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) < 30)
	{
		bWallRunIsRight = false;
	}
	// Right Cast
	else
	{		
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, RightEnd, "BlockAll", Params);
	
		if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) < 30)
		{
			bWallRunIsRight = true;
		}
		else
		{	
			return false;
		}
	}

	SetMovementMode(MOVE_Custom, CMOVE_WallRun);
	WallRunDuration = WallRunMaxDuration;
	bWallRunForward = FVector::DotProduct(CharacterOwner->GetActorForwardVector(), Velocity.GetSafeNormal()) > 0;	
	
	return true;
}

/**
 *  CustomPhys, allows the player to WallRun and LateJump
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::PhysWallRun(float deltaTime, int32 Iterations)
{	
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}
	
	bJustTeleported = false;
	float remainingTime = deltaTime;
	WallRunDuration -= deltaTime;
	
	// Perform the move	
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();		
		FVector Start = UpdatedComponent->GetComponentLocation();

		//Build Vector
		const FVector SideVector = FVector::CrossProduct(Velocity.GetSafeNormal(), FVector::DownVector) *
			((bWallRunIsRight) ? 1.f : -1.f) *
				((bWallRunForward) ? 1.f : -1.f);

		FCollisionQueryParams Params = ESCharacterOwner->GetIgnoreCharacterParams();		

		FVector VWallCheck = SideVector + Velocity.GetSafeNormal();
		VWallCheck *= CapsuleRScaled();

		// Check wall on forward and if not, on lateral
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, Start + VWallCheck,  "BlockAll", Params);

		if(!WallHit.IsValidBlockingHit())
		{			
			VWallCheck = SideVector;
			VWallCheck *= CapsuleRScaled();
			GetWorld()->LineTraceSingleByProfile(WallHit, Start, Start + VWallCheck,  "BlockAll", Params);			
		}	

		// Clamp Acceleration
		Acceleration = FVector::VectorPlaneProject(Acceleration, WallHit.Normal);
		Acceleration.Z = 0.f;
		
		// Apply acceleration
		CalcVelocity(timeTick, 0.f, false, GetMaxBrakingDeceleration());
		Velocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);

		// Apply wall tangential velocity
		FVector Tangent = FVector::CrossProduct(WallHit.Normal, FVector::UpVector);			
		if (FVector::DotProduct(Tangent, Velocity) < 0)
		{
			Tangent *= -1.f;
		}
		Velocity += Tangent * WallRunSpeedFactor;
		
#ifdef ENABLE_DEBUG_LINES
		DrawDebugLine(GetWorld(), Start, Start + SideVector  * 100.f, FColor::Blue, true, .5f, 0, 2.f);		
		DrawDebugLine(GetWorld(), Start, Start + VWallCheck, FColor::Red, true, .5f, 0, 3.f);
		DrawDebugLine(GetWorld(), Start, Start + FVector::DownVector * (CapsuleHH() + MinWallRunHeight), FColor::Green);
		DrawDebugLine(GetWorld(), Start, Start + Velocity.GetSafeNormal() * 100.f, FColor::Black, true, .5f, 0, 2.f);
#endif
		
		// Compute move parameters
		const FVector Delta = timeTick * Velocity; // dx = v * dt		
		if ( Delta.IsNearlyZero() )
		{
			remainingTime = 0.f;
		}
		else
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), false, Hit);			
			FVector WallAttractionDelta = -WallHit.Normal * WallAttractionForce * timeTick;
			SafeMoveUpdatedComponent(WallAttractionDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}

		//LateJump
		if (!WallHit.IsValidBlockingHit() ||  WallRunDuration < 0.f)
		{			
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			Safe_bWantsToWallRun = false;
			
			GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_LateJumpCooldown,
			this,
			&UEsLyraCharacterMovementComponent::OnLateJumpFinished,
			LateJumpDuration);
			bCanLateJump = true;
		
			return;
		}
		
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; // v = dx / dt		
	}
}

/**
 *  Reset the LateJump timer
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::OnLateJumpFinished()
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LateJumpCooldown);
	bCanLateJump = false;
}

#pragma endregion

#pragma region RewindTime

/**
 *  Fills the SavedPlayerStatusBuffer with player locations sampled in time
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::CollectRewindData(float deltaTime)
{
	CurrentSampleTime -= deltaTime;
	if(CurrentSampleTime <= 0.f)
	{
		FSavedPlayerStatus SavedPlayerStatus;        
		SavedPlayerStatus.SavedLocation = UpdatedComponent->GetComponentLocation();
		SavedPlayerStatus.SavedLife = LyraHealthComponent->GetHealth();
		PlayerStatusBuffer.Add(SavedPlayerStatus);		

		if(PlayerStatusBuffer.Num() >= BufferSampleMaxSize)
		{		
			PlayerStatusBuffer.RemoveAt(0);			
		}	

		CurrentSampleTime = RewindTimeSampleFrequencyTime;
	}
}

/**
 *  Move the character back to past locations with an Interpolation speed
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::PerformRewindingTime(float deltaTime)
{
	if(!bStartRewinding)
	{
		bStartRewinding = true;		
		RewindSampleTime = RewindingDuration / BufferSampleMaxSize;
		InterpolationSpeed = 1.f / RewindSampleTime;
		Safe_RewindingIndex = PlayerStatusBuffer.Num() - 1;
		if(Safe_RewindingIndex > 0)
			NewPosition = PlayerStatusBuffer[Safe_RewindingIndex].SavedLocation;
		CurrentRewindSampleTime = RewindSampleTime;	
	}		

	CurrentSampleTime = RewindTimeSampleFrequencyTime;
		
	if (Safe_RewindingIndex <= 0)
	{
		Safe_bIsRewinding = false;
		bStartRewinding = false;		
		PlayerStatusBuffer.Empty();					
		Velocity = FVector::ZeroVector;		
		SetMovementMode(MOVE_Falling);
		RewindTimeEndTime = GetWorld()->GetTimeSeconds();		
		return;
	}
	
	// Perform the move
	bJustTeleported = false;

	CurrentRewindSampleTime -= deltaTime;
	const FVector TargetPos = UKismetMathLibrary::VInterpTo(UpdatedComponent->GetComponentLocation(), NewPosition, deltaTime, InterpolationSpeed);
			
	// Compute move parameters
	const FVector Delta = TargetPos - UpdatedComponent->GetComponentLocation();	
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), false, Hit);		

	if(CurrentRewindSampleTime <= 0.f)
	{
		Safe_RewindingIndex--;	
		if(PlayerStatusBuffer.Num() > Safe_RewindingIndex)
		{		
			NewPosition = PlayerStatusBuffer[Safe_RewindingIndex].SavedLocation;	
		}			
		CurrentRewindSampleTime = RewindSampleTime;
	}
}

#pragma endregion 

#pragma region Interface

/**
 *  Returns the CustomMovementMode
 ****************************************************************************************/
bool UEsLyraCharacterMovementComponent::IsCustomMovementMode(const ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

/**
 *  Starts the teleportation. Called by Teleport GA
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::TeleportPressed()
{	
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if((CurrentTime < TeleportCooldownDuration) || (CurrentTime - TeleportStartTime >= TeleportCooldownDuration))
	{
		Safe_bWantsToTeleport = true;
	}	
}

/**
 *  Starts the Time Rewinding. Called by RewindTime GA
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::RewindTimePressed()
{	
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if((CurrentTime < RewindTimeCooldownDuration) || (CurrentTime - RewindTimeEndTime >= RewindTimeCooldownDuration - RewindingDuration))
	{	
		Safe_bIsRewinding = true;
		SetMovementMode(MOVE_Flying);

#ifdef ENABLE_DEBUG_LINES
		for (auto Element : SavedPlayerStatusBuffer)
		{
			DrawDebugSphere(GetWorld(), Element.SavedLocation, 5.f, 2, FColor::Green, true, -1.f, 0, 1);
		}	
#endif		
	}
}

/**
 *  Called on Jetpack pressed, activates Jetpack if the jetpack cooldown is over
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::JetpackPressed()
{
	UE_LOG(LogTemp, Log, TEXT("Jetpack key pressed"));

	if(!CanUseJetpack())
	{
			return;
	}

	
	
	if (const bool bIsClient = !CharacterOwner->HasAuthority() && CharacterOwner->IsLocallyControlled())
	{
		Server_ChangeJetpackStatus_Implementation(true);
	}

	Safe_bWantsToUseJetpack = true;
	
}

void UEsLyraCharacterMovementComponent::JetpackUnpressed()
{
	UE_LOG(LogTemp, Log, TEXT("Jetpack key released"));
	{
	}

	if (const bool bIsClient = !CharacterOwner->HasAuthority() && CharacterOwner->IsLocallyControlled())
	{
		Server_ChangeJetpackStatus_Implementation(false);
	}

	Safe_bWantsToUseJetpack = false;
}

float UEsLyraCharacterMovementComponent::GetRewindingTimeHealingMagnitude()
{	
	float MaxHealthInRewindingWindow = 0.f;
	for (const auto [SavedLocation, SavedLife] : PlayerStatusBuffer)
	{
		if(SavedLife > MaxHealthInRewindingWindow)
			MaxHealthInRewindingWindow = SavedLife;
	}
	return  MaxHealthInRewindingWindow - LyraHealthComponent->GetHealth();
}

#pragma endregion

#pragma region Replication

/**
 *  Returns the properties used for network replication
 ****************************************************************************************/
void UEsLyraCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(UEsLyraCharacterMovementComponent, WallRunDuration, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UEsLyraCharacterMovementComponent, bWallRunForward, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UEsLyraCharacterMovementComponent, bWallRunIsRight, COND_SkipOwner);
}

#pragma endregion

#pragma region GettersSetters

/**
 *  Returns Scaled Capsule Component radius of the character
 ****************************************************************************************/
float UEsLyraCharacterMovementComponent::CapsuleR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

/**
 *  Returns Scaled Capsule Component radius of the character, scaled by a factor
 ****************************************************************************************/
float UEsLyraCharacterMovementComponent::CapsuleRScaled() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * CapsuleScaleFactor;
}

/**
 *  Returns Scaled Capsule Component half height of the character
 ****************************************************************************************/
float UEsLyraCharacterMovementComponent::CapsuleHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

#pragma endregion


#pragma region SavedMove

FSavedMove_Es::FSavedMove_Es()
{
	Saved_bWantsToTeleport = 0;
	Saved_bWantsToWallRun = 0;
	Saved_bIsRewinding = 0;
	Saved_RewindingIndex = 0;
	Saved_bWantsToUseJetpack = 0;
}

void FSavedMove_Es::Clear()
{
	Super::Clear();

	Saved_bWantsToTeleport = 0;
	Saved_bWantsToWallRun = 0;
	Saved_bIsRewinding = 0;
	Saved_RewindingIndex = 0;
	Saved_bWantsToUseJetpack = 0;
}

/**
 *  Called to set up this saved move to make a predictive correction.
 ****************************************************************************************/
void FSavedMove_Es::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	const UEsLyraCharacterMovementComponent* CharacterMovement = Cast<UEsLyraCharacterMovementComponent>(C->GetCharacterMovement());

	Saved_bWantsToWallRun = CharacterMovement->Safe_bWantsToWallRun;
	Saved_bWantsToTeleport = CharacterMovement->Safe_bWantsToTeleport;
	Saved_bIsRewinding = CharacterMovement->Safe_bIsRewinding;
	Saved_RewindingIndex = CharacterMovement->Safe_RewindingIndex;
	Saved_bWantsToUseJetpack = CharacterMovement->Safe_bWantsToUseJetpack;
}

/**
 *  Returns true if this move can be combined with NewMove for replication without changing any behavior
 ****************************************************************************************/
bool FSavedMove_Es::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Es* NewEsMove = static_cast<FSavedMove_Es*>(NewMove.Get());

	if(Saved_bWantsToTeleport != NewEsMove->Saved_bWantsToTeleport)
	{
		return false;
	}

	if(Saved_bWantsToWallRun != NewEsMove->Saved_bWantsToWallRun)
	{
		return false;
	}
	
	if(Saved_bIsRewinding != NewEsMove->Saved_bIsRewinding)
	{
		return false;
	}

	if(Saved_RewindingIndex != NewEsMove->Saved_RewindingIndex)
	{
		return false;
	}

	if (Saved_bWantsToUseJetpack != NewEsMove->Saved_bWantsToUseJetpack)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

/**
 *  Called before ClientUpdatePosition uses this SavedMove to make a predictive correction
 ****************************************************************************************/
void FSavedMove_Es::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UEsLyraCharacterMovementComponent* CharacterMovement = Cast<UEsLyraCharacterMovementComponent>(C->GetCharacterMovement());

	CharacterMovement->Safe_bWantsToTeleport = Saved_bWantsToTeleport;
	
	CharacterMovement->Safe_bWantsToWallRun = Saved_bWantsToWallRun;

	CharacterMovement->Safe_bIsRewinding = Saved_bIsRewinding;
	CharacterMovement->Safe_RewindingIndex = Saved_RewindingIndex;	
	CharacterMovement->Safe_bWantsToUseJetpack = Saved_bWantsToUseJetpack;
}

/**
 *  Returns a byte containing encoded special movement information, plus custom ones
 ****************************************************************************************/
uint8 FSavedMove_Es::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if(Saved_bWantsToTeleport)
	{
		Result |= FLAG_Teleport;
	}

	if(Saved_bWantsToWallRun)
	{
		Result |= FLAG_WallRun;
	}
	
	if(Saved_bIsRewinding)
	{
		Result |= FLAG_RewindTime;
	}

	if (Saved_bWantsToUseJetpack)
	{
		Result |= FLAG_Jetpack;
	}

	
	return Result;
}

#pragma endregion 

#pragma region Client Network Prediction Data

FNetworkPredictionData_Client_Es::FNetworkPredictionData_Client_Es(const UCharacterMovementComponent& ClientMovement)
: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Es::AllocateNewMove()
{	
	return FSavedMovePtr(new FSavedMove_Es());
}

#pragma endregion


#pragma region Custom Move Data

/**
 *  Serialize the data in FEsNetworkMoveData struct
 ****************************************************************************************/
bool FEsNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, FCharacterNetworkMoveData::ENetworkMoveType MoveType)
{
	NetworkMoveType = MoveType;

	bool bLocalSuccess = true;
	const bool bIsSaving = Ar.IsSaving();

	Ar << TimeStamp;

	// TODO: better packing with single bit per component indicating zero/non-zero
	Acceleration.NetSerialize(Ar, PackageMap, bLocalSuccess);

	Location.NetSerialize(Ar, PackageMap, bLocalSuccess);

	// ControlRotation : FRotator handles each component zero/non-zero test; it uses a single signal bit for zero/non-zero, and uses 16 bits per component if non-zero.
	ControlRotation.NetSerialize(Ar, PackageMap, bLocalSuccess);

	SerializeOptionalValue<uint8>(bIsSaving, Ar, CompressedMoveFlags, 0);

	SerializeOptionalValue<UPrimitiveComponent*>(bIsSaving, Ar, MovementBase, nullptr);
	SerializeOptionalValue<FName>(bIsSaving, Ar, MovementBaseBoneName, NAME_None);
	SerializeOptionalValue<uint8>(bIsSaving, Ar, MovementMode, MOVE_Walking);
	
	Ar << Saved_RewindingIndex;
	
	return !Ar.IsError();
}

/**
 *  Fills in data in FEsNetworkMoveData struct with relevant movement data
 ****************************************************************************************/
void FEsNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
	FCharacterNetworkMoveData::ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FSavedMove_Es* CustomMove = static_cast<const FSavedMove_Es*>(&ClientMove);

	Saved_RewindingIndex = CustomMove->Saved_RewindingIndex;
}

#pragma endregion Custom Move Data
