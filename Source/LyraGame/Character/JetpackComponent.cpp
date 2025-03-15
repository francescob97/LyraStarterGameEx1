// Fill out your copyright notice in the Description page of Project Settings.


#include <Character/JetpackComponent.h>

#include "LyraCharacter.h"
#include "LyraLogChannels.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"

UJetpackComponent::UJetpackComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	CombatSet = nullptr;

	
}

void UJetpackComponent::InitializeWithAbilitySystem(ULyraAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		UE_LOG(LogLyra, Error, TEXT("LyraJetpackComponent: JetpackResource component for owner [%s] has already been initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogLyra, Error, TEXT("LyraJetpackComponent: Cannot initialize JetpackResource component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}

	CombatSet = AbilitySystemComponent->GetSet<ULyraCombatSet>();
	if (!CombatSet)
	{
		UE_LOG(LogLyra, Error, TEXT("LyraJetpackComponent: Cannot initialize Jetpack Resource component for owner [%s] with NULL Jetpack Resource set on the ability system."), *GetNameSafe(Owner));
		return;
	}

	// Register to listen for attribute changes.
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULyraCombatSet::GetJetpackResourceAttribute()).AddUObject(this, &ThisClass::HandleJetpackResourceChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULyraCombatSet::GetMaxJetpackResourceAttribute()).AddUObject(this, &ThisClass::HandleMaxJetpackResourceChanged);

	/* Gets Niagara Jetpack Component  */
	ALyraCharacter* ESCharacterOwner = Cast<ALyraCharacter>(Owner);
	if(const auto MeshComponent = ESCharacterOwner->GetMesh())
	{
		const auto JetpackRootMesh = MeshComponent->GetChildComponent(0);
		JetpackNiagaraComponent = dynamic_cast<UNiagaraComponent*>(JetpackRootMesh->GetChildComponent(0));
	}
	check(JetpackNiagaraComponent);
	
	if (const auto CapsuleComponent = ESCharacterOwner->GetCapsuleComponent())
	{
		const TArray<USceneComponent*>& ChildComponents = CapsuleComponent->GetAttachChildren();
		for (USceneComponent* ChildComponent : ChildComponents)
		{
			if (ChildComponent && ChildComponent->GetName() == FName("JetpackSFX"))
			{
				if (UAudioComponent* AudioComponent = Cast<UAudioComponent>(ChildComponent))
				{
					
					JetpackSFX = AudioComponent;
					break; 
				}
			}
		}
	}
	check(JetpackSFX);
}

void UJetpackComponent::UninitializeFromAbilitySystem()
{
	CombatSet = nullptr;
	AbilitySystemComponent = nullptr;
}

float UJetpackComponent::GetJetpackResource() const
{
	return (CombatSet ? CombatSet->GetJetpackResource() : 0.0f);
}

float UJetpackComponent::GetMaxJetpackResource() const
{
	return (CombatSet ? CombatSet->GetMaxJetpackResource() : 0.0f);
}

float UJetpackComponent::GetJetpackResourceNormalized() const
{
	if (CombatSet)
	{
		const float JetpackResource = CombatSet->GetJetpackResource();
		const float MaxJetpackResource = CombatSet->GetMaxJetpackResource();

		return ((MaxJetpackResource > 0.0f) ? (JetpackResource / MaxJetpackResource) : 0.0f);
	}

	return 0.0f;
}

void UJetpackComponent::SetJetpackEffects(const bool bActive) const
{
	if(!JetpackSFX)
	{
		UE_LOG(LogTemp,Warning, TEXT("Invalid Jetpack sound effect"));
		return;
	}

	if(!JetpackNiagaraComponent)
	{
		UE_LOG(LogTemp,Warning, TEXT("Invalid Jetpack Niagara component"));
		return;
	}

	if(bActive)
	{
		if(!JetpackSFX->IsPlaying())
		{
			JetpackSFX->Play();
		}
		if(!JetpackNiagaraComponent->IsActive())
			JetpackNiagaraComponent->Activate(true);
	}
	else
	{
		if(JetpackSFX->IsPlaying())
		{
			JetpackSFX->SetTriggerParameter(FName("JetpackOff"));
		}
		JetpackNiagaraComponent->Deactivate();
	}
}

bool UJetpackComponent::IsSpecialEffectOn()
{
	if(!JetpackNiagaraComponent)
		return false;
	
	return JetpackNiagaraComponent->IsActive();
}

void UJetpackComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();
	
	Super::OnUnregister();
}

void UJetpackComponent::HandleJetpackResourceChanged(const FOnAttributeChangeData& ChangeData)
{
	OnJetpackResourceChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}

void UJetpackComponent::HandleMaxJetpackResourceChanged(const FOnAttributeChangeData& ChangeData)
{
	OnMaxJetpackResourceChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}

