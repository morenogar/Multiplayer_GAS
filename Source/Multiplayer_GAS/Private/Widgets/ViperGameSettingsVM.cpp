// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperGameSettingsVM.h"
#include "Widgets/ViperSettingBaseVM.h"

void UViperGameSettingsVM::SetSettingsContainer(const TMap<FGameplayTag, UViperSettingBaseVM*>& NewSettingsContainer)
{
	SettingsContainer = NewSettingsContainer;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SettingsContainer);
}

bool UViperGameSettingsVM::ResetAllSettingsToDefaults()
{
	for (const TPair<FGameplayTag, UViperSettingBaseVM*>& SettingPair : SettingsContainer)
	{
		if (SettingPair.Value != nullptr)
		{
			SettingPair.Value->RevertToDefaults();
		}
	}
	return true;
}
