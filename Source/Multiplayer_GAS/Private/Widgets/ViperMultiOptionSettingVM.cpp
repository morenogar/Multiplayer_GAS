// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperMultiOptionSettingVM.h"

void UViperMultiOptionSettingVM::SetSelectedOptionIndex(const uint8 NewIndex)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedOptionIndex, NewIndex);
}

void UViperMultiOptionSettingVM::SetDefaultOptionIndex(const uint8 NewIndex)
{
	UE_MVVM_SET_PROPERTY_VALUE(DefaultOptionIndex, NewIndex);
}

void UViperMultiOptionSettingVM::SetOptionTexts(const TArray<FText>& NewOptionsTexts)
{
	UE_MVVM_SET_PROPERTY_VALUE(OptionTexts, NewOptionsTexts);
}

void UViperMultiOptionSettingVM::SetRequireConfirmation(const bool bNewRequireConfirmation)
{
	UE_MVVM_SET_PROPERTY_VALUE(bRequireConfirmation, bNewRequireConfirmation);
}

bool UViperMultiOptionSettingVM::RevertToDefaults()
{
	return UE_MVVM_SET_PROPERTY_VALUE(SelectedOptionIndex, DefaultOptionIndex);
}
