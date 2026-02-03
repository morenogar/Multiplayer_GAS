// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ViperInputConfig.generated.h"


class UInputAction;

USTRUCT(BlueprintType)
struct FViperInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	const UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(Categories="InputTag"))
	FGameplayTag InputTag;
};

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(TitleProperty="InputAction"))
	TArray<FViperInputAction> AbilityInputActions;

	
};
