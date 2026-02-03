// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperToggleSettingVM.h"

void UViperToggleSettingVM::SetToggleValue(const bool newToggleValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ToggleValue, newToggleValue);
}

void UViperToggleSettingVM::SetDefaultToggleValue(const bool newDefaultToggleValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(DefaultToggleValue, newDefaultToggleValue);
}

bool UViperToggleSettingVM::RevertToDefaults()
{
	return UE_MVVM_SET_PROPERTY_VALUE(ToggleValue, DefaultToggleValue);
}
