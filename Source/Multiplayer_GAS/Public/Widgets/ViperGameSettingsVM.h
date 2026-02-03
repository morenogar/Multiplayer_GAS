// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "ViperGameSettingsVM.generated.h"

class UViperSettingBaseVM;
/**
 * 
 */
UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperGameSettingsVM : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "Viper|SettingsVMs|GameSettings")
	const TMap<FGameplayTag, UViperSettingBaseVM*>& GetSettingsContainer() const { return SettingsContainer; }
	void SetSettingsContainer(const TMap<FGameplayTag, UViperSettingBaseVM*>& NewSettingsContainer);

	UFUNCTION(BlueprintCallable)
	bool ResetAllSettingsToDefaults();
	
private:
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	TMap<FGameplayTag, UViperSettingBaseVM*> SettingsContainer;

};
