// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/ViperSliderSettingVM.h"

void UViperSliderSettingVM::SetSliderValue(const float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(SliderValue, NewValue);
}

void UViperSliderSettingVM::SetMinValue(const float NewMinValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MinValue, NewMinValue);
}

void UViperSliderSettingVM::SetMaxValue(const float NewMaxValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxValue, NewMaxValue);
}

void UViperSliderSettingVM::SetStepSize(const float NewStepSize)
{
	UE_MVVM_SET_PROPERTY_VALUE(StepSize, NewStepSize);
}

void UViperSliderSettingVM::SetDefaultSliderValue(const float NewDefaultSliderValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(DefaultSliderValue, NewDefaultSliderValue);
}

bool UViperSliderSettingVM::RevertToDefaults()
{
	return UE_MVVM_SET_PROPERTY_VALUE(SliderValue, DefaultSliderValue);
}