// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperObjectDefinitionSubsystem.h"

#include "Engine/AssetManager.h"
#include "Interactables/ViperObjectDefinition.h"

UViperObjectDefinition* UViperObjectDefinitionSubsystem::GetObjectDefinition(const FPrimaryAssetId& assetID)
{
	if (ObjectDefinitionMap.Contains(assetID))
	{
		return ObjectDefinitionMap[assetID];
	}

	return nullptr;
}

void UViperObjectDefinitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UAssetManager& assetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> thingDefList;
	assetManager.GetPrimaryAssetIdList(FPrimaryAssetType(FName(TEXT("ViperObject"))),thingDefList);

	for (const FPrimaryAssetId& def : thingDefList)
	{
		FStreamableDelegate callback = FStreamableDelegate::CreateWeakLambda(this,[def, this]()
		{
			UAssetManager& assetManager = UAssetManager::Get();
			if (UObject* obj = assetManager.GetPrimaryAssetObject(def))
			{
				if (UViperObjectDefinition* thingDef = Cast<UViperObjectDefinition>(obj))
				{
					ObjectDefinitionMap.FindOrAdd(def) = thingDef;
				}
			}
			
			AssetLoadFinished();
		});
		assetManager.LoadPrimaryAsset(def, TArray<FName>(), callback);
	}
}

void UViperObjectDefinitionSubsystem::AssetLoadFinished()
{
}
