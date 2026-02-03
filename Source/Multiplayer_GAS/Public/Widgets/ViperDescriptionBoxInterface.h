// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperDescriptionBoxInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UViperDescriptionBoxInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperDescriptionBoxInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	//Default description box populate function
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|DescriptionBoxInterface")
	void UpdateDescriptionBox(UObject* caller, const FText& inTitle, const FText& inDescription);
};
