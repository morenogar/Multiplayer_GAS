// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperObjectDefinitionSubsystem.h"

#include "Engine/AssetManager.h"
#include "Interactables/ViperObjectDefinition.h"

UViperObjectDefinition* UViperObjectDefinitionSubsystem::GetObjectDefinition(const FPrimaryAssetId& AssetID)
{
	if (ObjectDefinitionMap.Contains(AssetID))
	{
		return ObjectDefinitionMap[AssetID];
	}

	return nullptr;
}

void UViperObjectDefinitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> ThingDefList;
	AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(FName(TEXT("ViperObject"))),ThingDefList);

	for (const FPrimaryAssetId& Def : ThingDefList)
	{
		FStreamableDelegate Callback = FStreamableDelegate::CreateWeakLambda(this,[Def, this]()
		{
			UAssetManager& AssetManager = UAssetManager::Get();
			if (UObject* Obj = AssetManager.GetPrimaryAssetObject(Def))
			{
				if (UViperObjectDefinition* ThingDef = Cast<UViperObjectDefinition>(Obj))
				{
					ObjectDefinitionMap.FindOrAdd(Def) = ThingDef;
				}
			}

			AssetLoadFinished();
		});
		AssetManager.LoadPrimaryAsset(Def, TArray<FName>(), Callback);
	}
}

void UViperObjectDefinitionSubsystem::AssetLoadFinished()
{
}
