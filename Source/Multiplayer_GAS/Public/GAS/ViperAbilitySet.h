// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ViperAbilitySet.generated.h"


struct FActiveGameplayEffectHandle;
struct FGameplayAbilitySpecHandle;
class UAttributeSet;
class UViperAbilitySystemComponent;
class UViperBaseGameplayAbility;


USTRUCT()
struct FViperAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void AddAbility(UViperAbilitySystemComponent* ViperAbilitySystemComponent, TSubclassOf<UViperBaseGameplayAbility> Ability, FGameplayTag InputTag = FGameplayTag());

protected:
	// Handles to the granted abilities.
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// Handles to the granted gameplay effects.
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	// Pointers to the granted attribute sets.
	UPROPERTY()
	TArray<UAttributeSet*> GrantedAttributeSets;
};

USTRUCT(BlueprintType)
struct FViperAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:
	// Gameplay ability to grant
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UViperBaseGameplayAbility> Ability = nullptr;

	// Level of ability to grant.
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	// Tag used to process input for the ability.
	UPROPERTY(EditDefaultsOnly, meta=(Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UViperAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void GiveToAbilitySystem(UViperAbilitySystemComponent* ViperAbilitySystemComponent, FViperAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category="Gameplay Abilities", meta=(TitleProperty=Ability))
	TArray<FViperAbilitySet_GameplayAbility> GrantedGameplayAbilities;
	
};
