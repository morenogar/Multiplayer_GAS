// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "Interactables/ViperInteractResponse.h"
#include "ViperInteractResponseFactory.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GASEDITOR_API UViperInteractResponseFactory : public UFactory
{
	GENERATED_BODY()

public:
	UViperInteractResponseFactory()
	{
		bCreateNew = true;
		bEditAfterNew = true;
		bEditorImport = false;
		SupportedClass = UViperInteractResponse::StaticClass();
		Formats.Add(TEXT("viper"));
	}

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		return NewObject<UViperInteractResponse>(InParent, Class, Name, Flags | RF_Transactional);
	}
	
	
	virtual bool FactoryCanImport(const FString& Filename) override
	{
		return false;
	}

	virtual bool ShouldShowInNewMenu() const
	{
		return true;
	}
};
