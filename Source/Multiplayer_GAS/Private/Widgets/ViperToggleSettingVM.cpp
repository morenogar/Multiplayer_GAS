// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperToggleSettingVM.h"

void UViperToggleSettingVM::SetToggleValue(const bool NewToggleValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bToggleValue, NewToggleValue);
}

void UViperToggleSettingVM::SetDefaultToggleValue(const bool NewDefaultToggleValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bDefaultToggleValue, NewDefaultToggleValue);
}

bool UViperToggleSettingVM::RevertToDefaults()
{
	return UE_MVVM_SET_PROPERTY_VALUE(bToggleValue, bDefaultToggleValue);
}
