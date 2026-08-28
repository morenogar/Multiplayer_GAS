// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Interactables/ViperInteractResponse.h"
#include "ViperInteractResponseFactory.h"


#define LOCTEXT_NAMESPACE "ViperEditor"

/**
 * 
 */
class FAssetTA_ViperInteractResponse : public FAssetTypeActions_Base
{
public:
	explicit FAssetTA_ViperInteractResponse(EAssetTypeCategories::Type InCategory)
	: AssetCategory(InCategory)
	{
	}
	
	virtual FText GetName() const override
	{
		return LOCTEXT("ViperInteractResponse", "Viper Interact Response");
	}

	virtual UClass* GetSupportedClass() const override
	{
		return UViperInteractResponse::StaticClass();
	}
	
	virtual FColor GetTypeColor() const override
	{
		return FColor(255,0,0);
	}
	
	virtual bool CanLocalize() const override { return false; }
	virtual uint32 GetCategories() override { return AssetCategory; }
	
	// End IAssetTypeActions Interface

private:
	EAssetTypeCategories::Type AssetCategory;
};

#undef LOCTEXT_NAMESPACE