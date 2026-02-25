// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperSettingBaseVM.h"

void UViperSettingBaseVM::SetSettingID(const FGameplayTag& NewSettingId)
{
	UE_MVVM_SET_PROPERTY_VALUE(SettingId, NewSettingId);
}

void UViperSettingBaseVM::SetSettingCategoryID(const FGameplayTag& NewSettingCategoryId)
{
	UE_MVVM_SET_PROPERTY_VALUE(SettingCategoryId, NewSettingCategoryId);
}

void UViperSettingBaseVM::SetSettingName(const FText& NewName)
{
	UE_MVVM_SET_PROPERTY_VALUE(SettingName, NewName);
}

void UViperSettingBaseVM::SetSettingDescription(const FText& NewDescription)
{
	UE_MVVM_SET_PROPERTY_VALUE(SettingDescription, NewDescription);
}

void UViperSettingBaseVM::SetDisabledDescription(const FText& NewDescription)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisabledDescription, NewDescription);
}

bool UViperSettingBaseVM::RevertToDefaults()
{
	return false;
}
