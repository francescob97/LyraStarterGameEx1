// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "UObject/ObjectRename.h"
#include "EsLyraCharacterMovementComponent.generated.h"

class UJetpackComponent;
class UNiagaraComponent;
class UAudioComponent;
class ULyraHealthComponent;
class ALyraCharacter;



/* Encapsulates a client move that is sent to the server for CMC networking */
struct FEsNetworkMoveData : public FCharacterNetworkMoveData
{
	uint8 MoreCompressedFlags;
	int32 Saved_RewindingIndex;
        
	FEsNetworkMoveData() : FCharacterNetworkMoveData(), MoreCompressedFlags(0), Saved_RewindingIndex(0)
	{
		Saved_RewindingIndex = 0;
	};
  
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, FCharacterNetworkMoveData::ENetworkMoveType MoveType) override;
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, FCharacterNetworkMoveData::ENetworkMoveType MoveType) override;
};

/* Used for network RPC parameters between client/serve*/
struct FEsNetworkMoveDataContainer: public FCharacterNetworkMoveDataContainer
{
	FEsNetworkMoveData CustomMoves[3];

	FEsNetworkMoveDataContainer() {
		NewMoveData = &CustomMoves[0];
		PendingMoveData = &CustomMoves[1];
		OldMoveData = &CustomMoves[2];
	}
};

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None			UMETA(Hidden),
	CMOVE_WallRun		UMETA(DisplayName = "Wall Run"),
	CMOVE_Jetpacking    UMETA(DisplayName = "Jetpacking"),
	CMOVE_MAX			UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct FSavedPlayerStatus
{
	GENERATED_BODY()

public:       
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SavedLocation = FVector::ZeroVector;  

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SavedLife = 0.f;
};

/**
 *  Custom character movement component class for the Lyra game.
 *  This class extends ULyraCharacterMovementComponent to add new movement abilities
 */
UCLASS()
class LYRAGAME_API UEsLyraCharacterMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UEsLyraCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** <UCharacterMovementComponent> */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	/** </UCharacterMovementComponent> */
	
	/** <ULyraCharacterMovementComponent> */
	virtual void InitializeComponent() override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves) override;
	/** </ULyraCharacterMovementComponent> */

	/*
	 *  Parameters
	 */

	UPROPERTY(Transient) 
	TObjectPtr<ALyraCharacter> ESCharacterOwner;
	
	UPROPERTY(Transient) 
	TObjectPtr<ULyraHealthComponent> LyraHealthComponent;
	
	// Distance the player will be teleported to in cm
	UPROPERTY(EditDefaultsOnly)
	float TeleportImpulse = 1000.f;

	UPROPERTY(EditDefaultsOnly)
	float TeleportCooldownDuration = 5.f;

	UPROPERTY(EditDefaultsOnly)
	float AuthTeleportCooldownDuration = 4.f;

	// Wall Run
	UPROPERTY(EditDefaultsOnly)
	float WallRunMaxDuration = 3.f;

	UPROPERTY(EditDefaultsOnly) 
	float WallRunSpeedFactor= 50.f;
	
	UPROPERTY(EditDefaultsOnly) 
	float WallAttractionForce = 300.f;
	
	UPROPERTY(EditDefaultsOnly) 
	float MinWallRunHeight = 50.f;
	
	UPROPERTY(EditDefaultsOnly) 
	float WallJumpOffForce = 300.f;

	UPROPERTY(EditDefaultsOnly)
	float LateJumpDuration = 1.f;

	//Rewind Time
	UPROPERTY(EditDefaultsOnly)
	float RewindTimeWindowDuration = 3.f;

	UPROPERTY(EditDefaultsOnly)
	float RewindTimeSampleFrequencyTime = .1f; 

	UPROPERTY(EditDefaultsOnly)
	float RewindingDuration = 1.25f;

	UPROPERTY(EditDefaultsOnly)
	float RewindTimeCooldownDuration = 12.f;

	UPROPERTY(EditDefaultsOnly)
	float AuthRewindTimeCooldownDuration = 11.f;
	

	//Jetpacking
	TObjectPtr<UJetpackComponent> JetpackComponent;
	
	/* When defining the velocity for jetpacking on z axes */
	UPROPERTY(EditDefaultsOnly, Category = "Custom|Jetpack")
	float JetpackVelocity;
	
	UPROPERTY(EditAnywhere, Category ="Custom|Jetpack")
	FGameplayTagContainer JetpackGameplayActionTag;
	

	UFUNCTION(BlueprintCallable)
	bool CanUseJetpack() const;
	
	virtual bool IsFalling() const override;
	
	/*
	 *  Flags (Transient)
	 */

	bool Safe_bWantsToTeleport;
	mutable bool Safe_bWantsToWallRun;
	bool Safe_bIsRewinding;
	int32 Safe_RewindingIndex;
	bool Safe_bWantsToUseJetpack;

	float TeleportStartTime;
	FTimerHandle TimerHandle_LateJumpCooldown;
	float RewindTimeEndTime;	
	
	/*
	 *  Replication
	 */

	UPROPERTY(Replicated)
	float WallRunDuration;
	
	/*
	 *  Delegates
	 */

protected:

	// Movement Pipeline
	/** <UCharacterMovementComponent> */
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	/** </UCharacterMovementComponent> */

private:

	FEsNetworkMoveDataContainer EsNetworkMoveDataContainer;

	/*
	 *  Teleport
	 */

	void PerformTeleport();

	/*
	 *  WallRun
	 */

	bool TryWallRun();
	void PhysWallRun(float deltaTime, int32 Iterations);
	void OnLateJumpFinished();	

	UPROPERTY(Replicated)
	bool bWallRunIsRight;
	
	UPROPERTY(Replicated)
	bool bWallRunForward;
	
	bool bCanLateJump;	
	FHitResult WallHit;
	float CapsuleScaleFactor = 3.f;

	/*
	 *  RewindTime
	 */

	void CollectRewindData(float deltaTime);
	void PerformRewindingTime(float deltaTime);
	
	TArray<FSavedPlayerStatus> PlayerStatusBuffer;
	
	int32 BufferSampleMaxSize;
	float CurrentSampleTime = 0.f;
	float RewindSampleTime;
	float CurrentRewindSampleTime;		

	FVector OldPosition;
	FVector NewPosition;
	float InterpolationSpeed;

	bool bStartRewinding = false;
	
	/*
	* Jetpacking
	*/
	
    /** Cancel jetpack ability using the tag specified in the blueprint through the parameter JetpackGameplayActionTag.
     * returns true if the ability was canceled false otherwise
     */
	bool CancelJetpackGameplayAbility() const;
	
	void PhysJetpacking(float deltaTime, int32 Iterations);
	
	UFUNCTION(Reliable, Server, WithValidation)
	void Server_ChangeJetpackStatus(bool bIsJetpackON);



protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	/*
	 *  Network
	 */
	
	/** <UCharacterMovementComponent> */

	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	/** </UCharacterMovementComponent> */
	
public:
	/*
	 *  Interface
	 */

	UFUNCTION(BlueprintPure)
	bool IsCustomMovementMode(const ECustomMovementMode InCustomMovementMode) const;

	UFUNCTION(BlueprintCallable)
	bool CanTeleport() const;

	UFUNCTION(BlueprintCallable)
	void TeleportPressed();

	/* Function called  by GA_Hero_Jetpack when the keymap for the jetpack is pressed */
	UFUNCTION(BlueprintCallable)
	void JetpackPressed();

	/* Function called when the keymap for jetpack is released */
	UFUNCTION(BlueprintCallable)
	void JetpackUnpressed();
		
	UFUNCTION(BlueprintCallable)
	void SetJumpEnd();
	
	UFUNCTION(BlueprintPure) FORCEINLINE
	bool IsWallRunning() const { return IsCustomMovementMode(CMOVE_WallRun); }
	
	UFUNCTION(BlueprintPure) FORCEINLINE
	bool WallRunningIsRight() const { return bWallRunIsRight; }

	UFUNCTION(BlueprintPure)
	float GetLookingAtAngle() const;
	
	UFUNCTION(BlueprintPure) FORCEINLINE
	bool GetIsGoingForward() const {return bWallRunForward; };

	FORCEINLINE bool CanWallJump() const {return IsWallRunning(); };

	FORCEINLINE bool CanLateJump() const { return bCanLateJump; };

	UFUNCTION(BlueprintCallable)
	void RewindTimePressed();

	UFUNCTION(BlueprintCallable)
	float GetRewindingTimeHealingMagnitude();

	/*
	 *  Proxy Replication
	 */

	/** </UActorComponent> */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** </UActorComponent> */

private:
	
	/*
	 *  Getter/Setters
	 */
	uint8 PrevPrevCustomMovementMode = 0;
	float CapsuleR() const;
	float CapsuleRScaled() const;
	float CapsuleHH() const;
	
};

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */
class LYRAGAME_API FSavedMove_Es : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FSavedMove_Es();
	
	// Bit masks used by GetCompressedFlags() to encode movement information.
	enum CompressedFlags
	{
		// Remaining bit masks are available for custom flags.
		FLAG_Teleport = 0x10, // Teleport pressed
		FLAG_WallRun = 0x20, // Wallrun pressed
		FLAG_RewindTime = 0x40, // RewindTime pressed
		FLAG_Jetpack = 0x80, // Jetpack pressed
	};
	
	/*
	 *  Flags
	 */
	
	uint8 Saved_bWantsToTeleport:1;
	uint8 Saved_bWantsToWallRun:1;
	uint8 Saved_bIsRewinding:1;
	uint8 Saved_bWantsToUseJetpack : 1;

	int32 Saved_RewindingIndex;

	/** <FSavedMove_Es> */
	
	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	/** Called to set up this saved move (when initially created) to make a predictive correction. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;	

	/** Returns true if this move can be combined with NewMove for replication without changing any behavior */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Called before ClientUpdatePosition uses this SavedMove to make a predictive correction	 */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** Returns a byte containing encoded special movement information (jumping, crouching, etc.)	 */
	virtual uint8 GetCompressedFlags() const override;
	
	/** </FSavedMove_Es> */
};


class FNetworkPredictionData_Client_Es : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;
	
	FNetworkPredictionData_Client_Es(const UCharacterMovementComponent& ClientMovement);
	
	/** <FNetworkPredictionData_Client_Es> */
	virtual FSavedMovePtr AllocateNewMove() override;
	/** </FNetworkPredictionData_Client_Es> */
};