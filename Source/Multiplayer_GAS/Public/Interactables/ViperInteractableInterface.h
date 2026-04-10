// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperInteractableInterface.generated.h"

class UViperObjectDefinition;
// This class does not need to be modified.

UINTERFACE(BlueprintType, NotBlueprintable)
class UViperInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperInteractableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	UFUNCTION(BlueprintCallable)
	virtual void Interact(APlayerController* Player) = 0;
};
