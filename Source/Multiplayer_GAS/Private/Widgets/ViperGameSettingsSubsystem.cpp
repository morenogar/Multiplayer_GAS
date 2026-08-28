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

void UViperGameSettingsSubsystem::PopulateSettingsVM(UViperGameSettingsVM& SettingsVM)
{
	//Input
	PopulateInputSettingsGeneral(SettingsVM);

	//Audio
	PopulateAudioSettings(SettingsVM);

	//Visuals
	PopulateVisualsSettings(SettingsVM);
}

void UViperGameSettingsSubsystem::PopulateInputSettingsGeneral(UViperGameSettingsVM& SettingsVM)
{
	if (GEngine == nullptr) return;

	if (UViperGameUserSettings* UserSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		TArray<FText>ToggleMultiOptionValues = {
			GAME_SETTINGS_LOC("Common_Option_On"),
			GAME_SETTINGS_LOC("Common_Option_Off")
		};



		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(UserSettings->GetMouseInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(UserSettings->GetMouseInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertMouseY, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(UserSettings->GetMouseInvertY(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertStickX, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(UserSettings->GetStickInvertX(), 1,ToggleMultiOptionValues, false));
		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Input, InvertStickY, TAG_SettingCategoryID_Input), FMultiOptionSettingInfo(UserSettings->GetStickInvertY(), 1,ToggleMultiOptionValues, false));

		constexpr float SensDefault = 5.0f;
		constexpr float SensMin = 0.1f;
		constexpr float SensMax = 10.0f;
		constexpr float StepSize = 1.0f;

		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, MouseLookSensitivityX, TAG_SettingCategoryID_Input), FSliderSettingInfo(UserSettings->GetMouseLookSensitivityX(), SensDefault, SensMin, SensMax, StepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, MouseLookSensitivityY, TAG_SettingCategoryID_Input), FSliderSettingInfo(UserSettings->GetMouseLookSensitivityY(), SensDefault, SensMin, SensMax, StepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, StickLookSensitivityX, TAG_SettingCategoryID_Input), FSliderSettingInfo(UserSettings->GetStickLookSensitivityX(), SensDefault, SensMin, SensMax, StepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Input, StickLookSensitivityY, TAG_SettingCategoryID_Input), FSliderSettingInfo(UserSettings->GetStickLookSensitivityY(), SensDefault, SensMin, SensMax, StepSize));
	}
}

void UViperGameSettingsSubsystem::PopulateAudioSettings(UViperGameSettingsVM& SettingsVM)
{
	if (GEngine == nullptr) return;

	if (UViperGameUserSettings* UserSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		constexpr float SensDefault = 0.5f;
		constexpr float SensMin = 0.0f;
		constexpr float SensMax = 5.0f;
		constexpr float StepSize = 1.0f;

		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Audio, MainVolume, TAG_SettingCategoryID_Volume), FSliderSettingInfo(UserSettings->GetMainVolume(), SensDefault, SensMin, SensMax, StepSize));
		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Audio, MusicVolume, TAG_SettingCategoryID_Volume), FSliderSettingInfo(UserSettings->GetMusicVolume(), SensDefault, SensMin, SensMax, StepSize));
	}
}

void UViperGameSettingsSubsystem::PopulateVisualsSettings(UViperGameSettingsVM& SettingsVM)
{
	if (GEngine == nullptr) return;

	if (UViperGameUserSettings* UserSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		TArray<FText>ToggleMultiOptionValues = {
			GAME_SETTINGS_LOC("Common_Option_Quality0"),
			GAME_SETTINGS_LOC("Common_Option_Quality1"),
			GAME_SETTINGS_LOC("Common_Option_Quality2"),
			GAME_SETTINGS_LOC("Common_Option_Quality3")
		};

		CreateAndRegisterSettingMultiOption(GAME_SETTINGS_BASIC_INFO_EX(Visuals, GeneralQuality, TAG_SettingCategoryID_Graphics), FMultiOptionSettingInfo(UserSettings->GetGeneralQuality(), 2,ToggleMultiOptionValues, true));

		constexpr float SensDefault = 80.0f;
		constexpr float SensMin = 70.0f;
		constexpr float SensMax = 100.0f;
		constexpr float StepSize = 1.0f;

		CreateAndRegisterSettingSlider(GAME_SETTINGS_BASIC_INFO_EX(Visuals, CameraFOV, TAG_SettingCategoryID_Graphics), FSliderSettingInfo(UserSettings->GetCameraFOV(), SensDefault, SensMin, SensMax, StepSize));
	}
}

void UViperGameSettingsSubsystem::CreateAndRegisterSettingSlider(FBasicSettingInfo SettingInfo,
	FSliderSettingInfo SliderSettingInfo)
{
	if (GameSettingsVM == nullptr)
		return;

	UViperSliderSettingVM* SliderVM = NewObject<UViperSliderSettingVM>();
	SliderVM->SetSettingID(SettingInfo.Id);
	SliderVM->SetSettingName(SettingInfo.Name);
	SliderVM->SetSettingDescription(SettingInfo.Description);
	SliderVM->SetSettingCategoryID(SettingInfo.CategoryID);

	SliderVM->SetSliderValue(SliderSettingInfo.CurrentValue);
	SliderVM->SetDefaultSliderValue(SliderSettingInfo.DefaultValue);
	SliderVM->SetMinValue(SliderSettingInfo.MinValue);
	SliderVM->SetMaxValue(SliderSettingInfo.MaxValue);
	SliderVM->SetStepSize(SliderSettingInfo.StepSize);

	GameSettingsVM->AddGameSettingEntry(SettingInfo.Id, SliderVM);

	const UE::FieldNotification::FFieldMulticastDelegate::FDelegate SliderChangedDelegate = UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UViperGameSettingsSubsystem::OnSettingSliderChanged);
	SliderVM->AddFieldValueChangedDelegate(UViperSliderSettingVM::FFieldNotificationClassDescriptor::SliderValue, SliderChangedDelegate);
}

void UViperGameSettingsSubsystem::CreateAndRegisterSettingMultiOption(FBasicSettingInfo SettingInfo,
	FMultiOptionSettingInfo MultiOptionSettingInfo)
{
	if (GameSettingsVM == nullptr)
		return;

	UViperMultiOptionSettingVM* MultiOptionVM = NewObject<UViperMultiOptionSettingVM>();
	MultiOptionVM->SetSettingID(SettingInfo.Id);
	MultiOptionVM->SetSettingName(SettingInfo.Name);
	MultiOptionVM->SetSettingDescription(SettingInfo.Description);
	MultiOptionVM->SetSettingCategoryID(SettingInfo.CategoryID);

	MultiOptionVM->SetSelectedOptionIndex(MultiOptionSettingInfo.CurrentOptionIndex);
	MultiOptionVM->SetDefaultOptionIndex(MultiOptionSettingInfo.DefaultOptionIndex);
	MultiOptionVM->SetOptionTexts(MultiOptionSettingInfo.OptionTexts);
	MultiOptionVM->SetRequireConfirmation(MultiOptionSettingInfo.bDoesOptionRequireConfirmation);

	GameSettingsVM->AddGameSettingEntry(SettingInfo.Id, MultiOptionVM);

	const UE::FieldNotification::FFieldMulticastDelegate::FDelegate MultiOptionChangedDelegate =UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UViperGameSettingsSubsystem::OnSettingMultiOptionChanged);
	MultiOptionVM->AddFieldValueChangedDelegate(UViperMultiOptionSettingVM::FFieldNotificationClassDescriptor::SelectedOptionIndex, MultiOptionChangedDelegate);
}

void UViperGameSettingsSubsystem::OnSettingSliderChanged(UObject* VMObject, UE::FieldNotification::FFieldId FieldId)
{
	if (UViperSliderSettingVM* SliderSettingVM = Cast<UViperSliderSettingVM>(VMObject))
	{
		const FGameplayTag SettingId = SliderSettingVM->GetSettingId();
		const float SliderValue = SliderSettingVM->GetSliderValue();

		if (UViperGameUserSettings* UserSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if(SettingId == TAG_GameSettingsID_CameraFOV)
			{
				UserSettings->SetCameraFOV(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_MouseLookSensitivityX)
			{
				UserSettings->SetMouseLookSensitivityX(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_MouseLookSensitivityY)
			{
				UserSettings->SetMouseLookSensitivityY(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_StickLookSensitivityX)
			{
				UserSettings->SetStickLookSensitivityX(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_StickLookSensitivityY)
			{
				UserSettings->SetStickLookSensitivityY(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_MainVolume)
			{
				UserSettings->SetMainVolume(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (SettingId == TAG_GameSettingsID_MusicVolume)
			{
				UserSettings->SetMusicVolume(SliderValue);
				UserSettings->ApplyNonResolutionSettings();
			}
		}

		OnSettingUpdated.Broadcast(SettingId);
	}
}

void UViperGameSettingsSubsystem::OnSettingMultiOptionChanged(UObject* VMObject,UE::FieldNotification::FFieldId FieldId)
{

	if (UViperMultiOptionSettingVM* MultiOptionVM = Cast<UViperMultiOptionSettingVM>(VMObject))
	{
		const FGameplayTag MultiOptionId = MultiOptionVM->GetSettingId();
		const bool MultiOptionIndexBool = static_cast<bool>(MultiOptionVM->GetSelectedOptionIndex());
		const int32 MultiOptionIndex = static_cast<int32>(MultiOptionVM->GetSelectedOptionIndex());
		const TArray<FText>& OptionTexts = MultiOptionVM->GetOptionTexts();
		const bool bIsValidIndex = OptionTexts.IsValidIndex(MultiOptionIndex);

		if (bIsValidIndex == false) return;

		if (UViperGameUserSettings* UserSettings = Cast<UViperGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if (MultiOptionId == TAG_GameSettingsID_InvertMouseX)
			{
				UserSettings->SetMouseInvertX(MultiOptionIndexBool);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (MultiOptionId == TAG_GameSettingsID_InvertMouseY)
			{
				UserSettings->SetMouseInvertY(MultiOptionIndexBool);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (MultiOptionId == TAG_GameSettingsID_InvertStickX)
			{
				UserSettings->SetStickInvertX(MultiOptionIndexBool);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (MultiOptionId == TAG_GameSettingsID_InvertStickY)
			{
				UserSettings->SetStickInvertY(MultiOptionIndexBool);
				UserSettings->ApplyNonResolutionSettings();
			}
			else if (MultiOptionId == TAG_GameSettingsID_GeneralQuality)
			{
				UserSettings->SetGeneralQuality(MultiOptionIndex);
				UserSettings->ApplyNonResolutionSettings();
			}
		}

		OnSettingUpdated.Broadcast(MultiOptionId);
	}
}
