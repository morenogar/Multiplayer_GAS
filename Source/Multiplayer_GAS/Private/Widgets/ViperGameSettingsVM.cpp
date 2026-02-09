// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperGameSettingsVM.h"
#include "Widgets/ViperSettingBaseVM.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsIDRoot, "GameSetting");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_DisplayMode, "GameSetting.DisplayMode");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_VsyncEnabled, "GameSetting.VSyncEnabled");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MotionBlur, "GameSetting.MotionBlurEnabled");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_Brightness, "GameSetting.Brightness");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_Contrast, "GameSetting.Contrast");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_HDREnabled, "GameSetting.HDREnabled");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_Resolution, "GameSetting.Resolution");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_FrameRateLimit, "GameSetting.FrameRateLimit");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_Upscaler, "GameSetting.Upscaler");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_PostProcessingQuality, "GameSetting.PostProcessingQuality");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_ShadowQuality, "GameSetting.ShadowQuality");

UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MonoAudio, "GameSetting.MonoAudio");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MainVolume, "GameSetting.MainVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MusicVolume, "GameSetting.MusicVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_SoundEffectsVolume, "GameSetting.SoundEffectsVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_AmbientEffectsVolume, "GameSetting.AmbientEffectsVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_CharacterVoiceVolume, "GameSetting.CharacterVoiceVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MouseLookSensitivityX, "GameSetting.MouseLookSensitivityX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MouseLookSensitivityY, "GameSetting.MouseLookSensitivityY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MouseAimSensitivityX, "GameSetting.MouseAimSensitivityX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_MouseAimSensitivityY, "GameSetting.MouseAimSensitivityY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_InvertMouseX, "GameSetting.InvertMouseX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_InvertMouseY, "GameSetting.InvertMouseY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickLookSensitivityX, "GameSetting.StickLookSensitivityX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickLookSensitivityY, "GameSetting.StickLookSensitivityY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickAimSensitivityX, "GameSetting.StickAimSensitivityX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickAimSensitivityY, "GameSetting.StickAimSensitivityY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_InvertStickX, "GameSetting.InvertStickX");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_InvertStickY, "GameSetting.InvertStickY");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickInnerDeadZone, "GameSetting.StickInnerDeadZone");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_StickOuterLimit, "GameSetting.StickOuterLimit");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_ControllerVibrationEnabled, "GameSetting.ControllerVibrationEnabled");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_ControllerVibrationIntensity, "GameSetting.ControllerVibrationIntensity");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_Language, "GameSetting.Language");
UE_DEFINE_GAMEPLAY_TAG(TAG_GameSettingsID_TextSize, "GameSetting.TextSize");

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
