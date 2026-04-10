// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ViperObjectDefinition.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperObjectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("ViperObject", FPackageName::GetShortFName(GetName()));
	}
	
	UFUNCTION(BlueprintPure, Category="ViperObjectDefinition")
	virtual FText GetDisplayName() const;
	
	UFUNCTION(BlueprintPure, Category="ViperObjectDefinition")
	virtual FText GetShortDescription() const;
	
	UFUNCTION(BlueprintPure, Category="ViperObjectDefinition")
	virtual const FGameplayTagContainer& GetGameplayTags() const;

	UFUNCTION(BlueprintPure, Category="ViperObjectDefinition")
	virtual TSoftObjectPtr<UTexture2D> GeIconSoftObject() const;
	
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer CategoryTags;
};
