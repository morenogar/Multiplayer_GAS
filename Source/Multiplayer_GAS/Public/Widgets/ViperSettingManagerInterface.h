// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperSettingManagerInterface.generated.h"

class UViperSettingInterface;

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UViperSettingManagerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperSettingManagerInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|SettingWidgetManagerInterface")
	void RegisterSettingWidget(const TScriptInterface<UViperSettingInterface>& SettingWidgetInterface);
};
