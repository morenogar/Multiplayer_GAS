// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ViperGASAbilityBlueprintLibrary.generated.h"

class UGameplayAbility;
struct FGameplayTagContainer;
class UAbilitySystemComponent;
class UViperAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperGASAbilityBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	
public:
	UFUNCTION(BlueprintPure, Category = Ability, Meta=(DefaultToSelf = "Actor"))
	static UViperAbilitySystemComponent* GetViperAbilitySystemComponent(AActor* Actor);

	UFUNCTION(BlueprintCallable)
	static void CancelAbilities(UAbilitySystemComponent* AbilitySystemComponent, FGameplayTagContainer WithTags, FGameplayTagContainer WithoutTags, UGameplayAbility* Ignore = nullptr);

	
};
