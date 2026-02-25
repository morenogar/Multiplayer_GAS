// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include <GameplayTagContainer.h>
#include "ViperSettingBaseVM.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperSettingBaseVM : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	FGameplayTag GetSettingId() const { return SettingId; }
	void SetSettingID(const FGameplayTag& NewSettingId);
	
	FGameplayTag GetSettingCategoryId() const { return SettingCategoryId; }
	void SetSettingCategoryID(const FGameplayTag& NewSettingCategoryId);

	const FText& GetSettingName() const { return SettingName; }
	void SetSettingName(const FText& NewName);

	const FText& GetSettingDescription() const { return SettingDescription; }
	void SetSettingDescription(const FText& NewDescription);

	const FText& GetDisabledDescription() const { return DisabledDescription; }
	void SetDisabledDescription(const FText& NewDescription);
	
	UFUNCTION(BlueprintCallable)
	virtual bool RevertToDefaults();
	
private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Viper|SettingsVMs|Base", meta=(AllowPrivateAccess=true))
	FGameplayTag SettingId;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Viper|SettingsVMs|Base", meta=(AllowPrivateAccess=true))
	FGameplayTag SettingCategoryId;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Viper|SettingsVMs|Base", meta=(AllowPrivateAccess=true))
	FText SettingName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Viper|SettingsVMs|Base", meta=(AllowPrivateAccess=true))
	FText SettingDescription;

	//To be used in conjunction with description if this setting is disabled, to explain why it's disabled
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Viper|SettingsVMs|Base", meta=(AllowPrivateAccess=true))
	FText DisabledDescription;
};
