// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ViperBaseGameplayAbility.generated.h"

class AViperPlayerController;
class AViperCharacter;

UENUM(BlueprintType)
enum class EViperAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered
	OnInputTriggered,

	// Continually try to activate the ability while the input is active.
	WhileInputActive,

	// Try to activate the ability when an avatar is assigned.
	OnSpawn,

	// activated by Gameplay Event or other external system, still will register for input events if it has a tag
	Manual,

	// Activated automatically when the ability is granted via Ability Set
	OnGranted,

	// Activated automatically when ability is added to ASC, regardless of source
	OnAbilityAdded
};

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	
	EViperAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	AViperCharacter* GetOwningCharacter() const;

	UFUNCTION(BlueprintPure)
	AViperPlayerController* GetOwningPlayerController() const;
	
protected:
	// Begin UGameplayAbilityInterface
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	// End UGameplayAbilityInterface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Viper|Ability Activation")
	EViperAbilityActivationPolicy ActivationPolicy;

	UFUNCTION(BlueprintImplementableEvent, Category=Ability, DisplayName="OnAbilityAdded")
	void BP_OnAbilityAdded();

	UFUNCTION(BlueprintImplementableEvent, Category=Ability, DisplayName="OnAbilityRemoved")
	void BP_OnAbilityRemoved();
	
	
};
