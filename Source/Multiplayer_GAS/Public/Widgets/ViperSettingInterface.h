// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperSettingInterface.generated.h"

class UViperSettingBaseVM;
struct FGameplayTag;

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UViperSettingInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperSettingInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|SettingWidgetInterface")
	bool GetSettingID(FGameplayTag& outIdTag) const;
	
	 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|SettingWidgetInterface")
	 void ConnectSettingVM(UViperSettingBaseVM* viewModelBase);
};
