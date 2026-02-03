// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ViperAbilitySystemComponent.generated.h"

class UViperAbilitySet;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UViperAbilitySystemComponent()
	: Super()
	  , bHasGrantedDefaultAbilities(false)
	{
	}
	
	void ApplyInitialEffects();

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	void GrantDefaultAbilities(UViperAbilitySet* AbilitySet);

	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

protected:
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// set when we call GrantDefaultAbilities the first time
	uint8 bHasGrantedDefaultAbilities : 1;

private:

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TArray<TSubclassOf<UGameplayEffect>> InitialEffects;
};
