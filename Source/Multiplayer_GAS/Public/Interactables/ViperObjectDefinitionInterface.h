// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperObjectDefinitionInterface.generated.h"

class UViperObjectDefinition;


UINTERFACE(BlueprintType, NotBlueprintable)
class UViperObjectDefinitionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperObjectDefinitionInterface
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable)
	virtual const UViperObjectDefinition* GetObjectDefinition() const = 0;
};
