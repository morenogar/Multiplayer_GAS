// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperGameSettingsSubsystem.h"

#include "Framework/ViperGameUserSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/ViperGameSettingsVM.h"
#include "Widgets/ViperMultiOptionSettingVM.h"
#include "Widgets/ViperSliderSettingVM.h"

static const FName StringTable_GameSettings = "/Game/UI/StringTables/ST_Setting";
#define GAME_SETTINGS_LOC(stringTableKey)	FText::FromStringTable(StringTable_GameSettings, FTextKey(stringTableKey), EStringTableLoadingPolicy::FindOrFullyLoad)

#define KEYSTRING_SETTING_NAME(category, setting)		#category "_Setting_" #setting
#define KEYSTRING_SETTING_DESC(category, setting)		#category "_Description_" #setting

#define GAME_SETTINGS_BASIC_INFO_EX(category, setting, categortyID)	FBasicSettingInfo(TAG_GameSettingsID_##setting,				\
FText::FromStringTable(StringTable_GameSettings, FTextKey(KEYSTRING_SETTING_NAME(category, setting)), EStringTableLoadingPolicy::FindOrFullyLoad),	\
FText::FromStringTable(StringTable_GameSettings, FTextKey(KEYSTRING_SETTING_DESC(category, setting)), EStringTableLoadingPolicy::FindOrFullyLoad), \
categortyID)

void UViperGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GetGameSettingsViewModel();
}

UViperGameSettingsVM* UViperGameSettingsSubsystem::GetGameSettingsViewModel()
{
	if (GameSettingsVM != nullptr)
		return GameSettingsVM;

	GameSettingsVM = NewObject<UViperGameSettingsVM>();
	PopulateSettingsVM(*GameSettingsVM);
	
	return GameSettingsVM;
}

void UViperGameSettingsSubsystem::PopulateSettingsVM(UViperGameSettingsVM& settingsVM)
{
	//Input
	PopulateInputSettingsGeneral(settingsVM);
	
	//Audio
	PopulateAudioSettings(settingsVM);
	
	//Visuals
	PopulateVisualsSettings(settingsVM);
}

void UViperGameSettingsSubsystem::PopulateInputSettingsGeneral(UViperGameSettingsVM& settingsVM)
{
	if (GEngine == nullptr) return;
	
	if (UViperGameUserSettings* userSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		TArray<FText>ToggleMultiOptionValues = {
			GAME_SETTINGS_LOC("Common_Option_On"),
			GAME_SETTINGS_LOC("Common_Option_Off")
		};
		
		
		
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(userSettings->GetMouseInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(userSettings->GetMouseInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseY, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(userSettings->GetMouseInvertY(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertStickX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(userSettings->GetStickInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertStickY, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(userSettings->GetStickInvertY(), 1,ToggleMultiOptionValues, false));
		
		constexpr float sensDefault = 5.0f;
		constexpr float sensMin = 0.1f;
		constexpr float sensMax = 10.0f;
		constexpr float stepSize = 1.0f;
		
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, MouseLookSensitivityX, TAG_SettingCategoryID_Input), FSliderSettingInfo(userSettings->GetMouseLookSensitivityX(), sensDefault, sensMin, sensMax, stepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, MouseLookSensitivityY, TAG_SettingCategoryID_Input), FSliderSettingInfo(userSettings->GetMouseLookSensitivityY(), sensDefault, sensMin, sensMax, stepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, StickLookSensitivityX, TAG_SettingCategoryID_Input), FSliderSettingInfo(userSettings->GetStickLookSensitivityX(), sensDefault, sensMin, sensMax, stepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, StickLookSensitivityY, TAG_SettingCategoryID_Input), FSliderSettingInfo(userSettings->GetStickLookSensitivityY(), sensDefault, sensMin, sensMax, stepSize));
	}
}

void UViperGameSettingsSubsystem::PopulateAudioSettings(UViperGameSettingsVM& settingsVM)
{
	if (GEngine == nullptr) return;
	
	if (UViperGameUserSettings* userSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		constexpr float sensDefault = 0.5f;
		constexpr float sensMin = 0.0f;
		constexpr float sensMax = 5.0f;
		constexpr float stepSize = 1.0f;
		
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Audio, MainVolume, TAG_SettingCategoryID_Volume), FSliderSettingInfo(userSettings->GetMainVolume(), sensDefault, sensMin, sensMax, stepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Audio, MusicVolume, TAG_SettingCategoryID_Volume), FSliderSettingInfo(userSettings->GetMusicVolume(), sensDefault, sensMin, sensMax, stepSize));
	}
}

void UViperGameSettingsSubsystem::PopulateVisualsSettings(UViperGameSettingsVM& settingsVM)
{
	if (GEngine == nullptr) return;
	
	if (UViperGameUserSettings* userSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		TArray<FText>ToggleMultiOptionValues = {
			GAME_SETTINGS_LOC("Common_Option_Quality0"),
			GAME_SETTINGS_LOC("Common_Option_Quality1"),
			GAME_SETTINGS_LOC("Common_Option_Quality2"),
			GAME_SETTINGS_LOC("Common_Option_Quality3")
		};
		
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Visuals, GeneralQuality, TAG_SettingCategoryID_Graphics), FMultiOptionSettingInfo(userSettings->GetGeneralQuality(), 2,ToggleMultiOptionValues, true));
		
		constexpr float sensDefault = 80.0f;
		constexpr float sensMin = 70.0f;
		constexpr float sensMax = 100.0f;
		constexpr float stepSize = 1.0f;
		
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Visuals, CameraFOV, TAG_SettingCategoryID_Graphics), FSliderSettingInfo(userSettings->GetCameraFOV(), sensDefault, sensMin, sensMax, stepSize));
	}
}

void UViperGameSettingsSubsystem::CreateAndRegisterSettingSlider(FBasicSettingInfo settingInfo,
	FSliderSettingInfo sliderSettingInfo)
{
	if (GameSettingsVM == nullptr)
		return;
	
	UViperSliderSettingVM* sliderVM = NewObject<UViperSliderSettingVM>();
	sliderVM->SetSettingID(settingInfo.Id);
	sliderVM->SetSettingName(settingInfo.Name);
	sliderVM->SetSettingDescription(settingInfo.Description);
	sliderVM->SetSettingCategoryID(settingInfo.CategoryID);

	sliderVM->SetSliderValue(sliderSettingInfo.CurrentValue);
	sliderVM->SetDefaultSliderValue(sliderSettingInfo.DefaultValue);
	sliderVM->SetMinValue(sliderSettingInfo.MinValue);
	sliderVM->SetMaxValue(sliderSettingInfo.MaxValue);
	sliderVM->SetStepSize(sliderSettingInfo.StepSize);
	
	GameSettingsVM->AddGameSettingEntry(settingInfo.Id, sliderVM);
	
	const UE::FieldNotification::FFieldMulticastDelegate::FDelegate sliderChangedDelegate = UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UViperGameSettingsSubsystem::OnSettingSliderChanged);
	sliderVM->AddFieldValueChangedDelegate(UViperSliderSettingVM::FFieldNotificationClassDescriptor::SliderValue, sliderChangedDelegate);
}

void UViperGameSettingsSubsystem::CreateAndRegisterSettingMultiOption(FBasicSettingInfo settingInfo,
	FMultiOptionSettingInfo multiOptionSettingInfo)
{
	if (GameSettingsVM == nullptr)
		return;
	
	UViperMultiOptionSettingVM* multiOptionVM = NewObject<UViperMultiOptionSettingVM>();
	multiOptionVM->SetSettingID(settingInfo.Id);
	multiOptionVM->SetSettingName(settingInfo.Name);
	multiOptionVM->SetSettingDescription(settingInfo.Description);
	multiOptionVM->SetSettingCategoryID(settingInfo.CategoryID);

	multiOptionVM->SetSelectedOptionIndex(multiOptionSettingInfo.CurrentOptionIndex);
	multiOptionVM->SetDefaultOptionIndex(multiOptionSettingInfo.DefaultOptionIndex);
	multiOptionVM->SetOptionTexts(multiOptionSettingInfo.OptionTexts);
	multiOptionVM->SetRequireConfirmation(multiOptionSettingInfo.DoesOptionRequireConfirmation);
	
	GameSettingsVM->AddGameSettingEntry(settingInfo.Id, multiOptionVM);

	const UE::FieldNotification::FFieldMulticastDelegate::FDelegate multiOptionChangedDelegate =UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UViperGameSettingsSubsystem::OnSettingMultiOptionChanged);
	multiOptionVM->AddFieldValueChangedDelegate(UViperMultiOptionSettingVM::FFieldNotificationClassDescriptor::SelectedOptionIndex, multiOptionChangedDelegate);
}

void UViperGameSettingsSubsystem::OnSettingSliderChanged(UObject* vmObject, UE::FieldNotification::FFieldId fieldId)
{
	if (UViperSliderSettingVM* sliderSettingVM = Cast<UViperSliderSettingVM>(vmObject))
	{
		const FGameplayTag settingId = sliderSettingVM->GetSettingId();
		const float sliderValue = sliderSettingVM->GetSliderValue();

		if (UViperGameUserSettings* userSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if(settingId == TAG_GameSettingsID_CameraFOV)
			{
				userSettings->SetCameraFOV(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_MouseLookSensitivityX)
			{
				userSettings->SetMouseLookSensitivityX(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_MouseLookSensitivityY)
			{
				userSettings->SetMouseLookSensitivityY(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_StickLookSensitivityX)
			{
				userSettings->SetStickLookSensitivityX(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_StickLookSensitivityY)
			{
				userSettings->SetStickLookSensitivityY(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_MainVolume)
			{
				userSettings->SetMainVolume(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (settingId == TAG_GameSettingsID_MusicVolume)
			{
				userSettings->SetMusicVolume(sliderValue);
				userSettings->ApplyNonResolutionSettings();
			}
		}

		OnSettingUpdated.Broadcast(settingId);
	}
}

void UViperGameSettingsSubsystem::OnSettingMultiOptionChanged(UObject* vmObject,UE::FieldNotification::FFieldId fieldId)
{
	
	if (UViperMultiOptionSettingVM* multiOptionVM = Cast<UViperMultiOptionSettingVM>(vmObject))
	{
		const FGameplayTag multiOptionId = multiOptionVM->GetSettingId();
		const bool multiOptionIndexBool = static_cast<bool>(multiOptionVM->GetSelectedOptionIndex());
		const int32 multiOptionIndex = static_cast<int32>(multiOptionVM->GetSelectedOptionIndex());
		const TArray<FText>& optionTexts = multiOptionVM->GetOptionTexts();
		const bool bIsValidIndex = optionTexts.IsValidIndex(multiOptionIndex);
		
		if (bIsValidIndex == false) return;
	
		if (UViperGameUserSettings* userSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if (multiOptionId == TAG_GameSettingsID_InvertMouseX)
			{
				userSettings->SetMouseInvertX(multiOptionIndexBool);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (multiOptionId == TAG_GameSettingsID_InvertMouseY)
			{
				userSettings->SetMouseInvertY(multiOptionIndexBool);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (multiOptionId == TAG_GameSettingsID_InvertStickX)
			{
				userSettings->SetStickInvertX(multiOptionIndexBool);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (multiOptionId == TAG_GameSettingsID_InvertStickY)
			{
				userSettings->SetStickInvertY(multiOptionIndexBool);
				userSettings->ApplyNonResolutionSettings();
			}
			else if (multiOptionId == TAG_GameSettingsID_GeneralQuality)
			{
				userSettings->SetGeneralQuality(multiOptionIndex);
				userSettings->ApplyNonResolutionSettings();
			}
		}
		
		OnSettingUpdated.Broadcast(multiOptionId);
	}
}
