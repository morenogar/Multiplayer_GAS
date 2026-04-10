// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ViperObjectDefinitionSubsystem.generated.h"

class UViperObjectDefinition;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperObjectDefinitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "AgoraThingSubsystem")
	UViperObjectDefinition* GetObjectDefinition(const FPrimaryAssetId& assetID);
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
protected:
	
	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UViperObjectDefinition>> ObjectDefinitionMap;

	void AssetLoadFinished();
};
