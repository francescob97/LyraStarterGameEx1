// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameFrameworkComponent.h"
#include "JetpackComponent.generated.h"

class UNiagaraComponent;
struct FOnAttributeChangeData;
class ULyraCombatSet;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJetpackResource_AttributeChanged, UJetpackComponent*, JetpackComponent, float,
											   OldValue, float, NewValue);

class ULyraAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class LYRAGAME_API UJetpackComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()
public:
	UJetpackComponent(const FObjectInitializer& ObjectInitializer);

	// Returns the Jetpack component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Lyra|Jetpack")
	static UJetpackComponent* FindJetpackComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UJetpackComponent>() : nullptr); }

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	void InitializeWithAbilitySystem(ULyraAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	void UninitializeFromAbilitySystem();

	// Returns the current Jetpack value.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	float GetJetpackResource() const;

	// Returns the current maximum Jetpack value.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	float GetMaxJetpackResource() const;

	// Returns the current Jetpack in the range [0.0, 1.0].
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	float GetJetpackResourceNormalized() const;

	UFUNCTION(BlueprintCallable)
	void SetJetpackEffects(const bool bActive) const;
	// Return true if the special effect for the jetpack component is on and playing 
	UFUNCTION(BlueprintCallable, Category = "Lyra|Jetpack")
	bool IsSpecialEffectOn();
public:

	// Delegate fired when the Jetpack value has changed.
	UPROPERTY(BlueprintAssignable)
	FJetpackResource_AttributeChanged OnJetpackResourceChanged;

	// Delegate fired when the max Jetpack value has changed.
	UPROPERTY(BlueprintAssignable)
	FJetpackResource_AttributeChanged OnMaxJetpackResourceChanged;
		
	UPROPERTY(EditDefaultsOnly, Category ="Custom|Jetpack")
	TObjectPtr<UNiagaraComponent> JetpackNiagaraComponent;

	UPROPERTY(EditDefaultsOnly, Category ="Custom|Jetpack")
	TObjectPtr<UAudioComponent> JetpackSFX;
	// UPROPERTY(BlueprintAssignable)
	// UGameplayEffectClass ResourceRegenerationGameplayEffectClass;
protected:
	virtual void OnUnregister() override;
	
	virtual void HandleJetpackResourceChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleMaxJetpackResourceChanged(const FOnAttributeChangeData& ChangeData);

	
	// Ability system used by this component.
	UPROPERTY()
	ULyraAbilitySystemComponent* AbilitySystemComponent;

	// Jetpack set used by this component.
	UPROPERTY()
	const ULyraCombatSet* CombatSet;
};
