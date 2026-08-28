// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperGameUserSettings.h"

//INPUT
void UViperGameUserSettings::SetMouseLookSensitivityX(float Sensitivity) {MouseLookSensitivityX = Sensitivity;}
float UViperGameUserSettings::GetMouseLookSensitivityX() const {return MouseLookSensitivityX;}
void UViperGameUserSettings::SetMouseLookSensitivityY(float Sensitivity) {MouseLookSensitivityY = Sensitivity;}
float UViperGameUserSettings::GetMouseLookSensitivityY() const {return MouseLookSensitivityY;}
void UViperGameUserSettings::SetMouseInvertX(bool bInvert) {bMouseInvertX = bInvert;}
bool UViperGameUserSettings::GetMouseInvertX() const {return bMouseInvertX;}
void UViperGameUserSettings::SetMouseInvertY(bool bInvert) {bMouseInvertY = bInvert;}
bool UViperGameUserSettings::GetMouseInvertY() const {return bMouseInvertY;}
void UViperGameUserSettings::SetStickLookSensitivityX(float Sensitivity) {StickLookSensitivityX = Sensitivity;}
float UViperGameUserSettings::GetStickLookSensitivityX() const {return StickLookSensitivityX;}
void UViperGameUserSettings::SetStickLookSensitivityY(float Sensitivity) {StickLookSensitivityY = Sensitivity;}
float UViperGameUserSettings::GetStickLookSensitivityY() const {return StickLookSensitivityY;}
void UViperGameUserSettings::SetStickInvertX(bool bInvert) {bStickInvertX = bInvert;}
bool UViperGameUserSettings::GetStickInvertX() const {return bStickInvertX;}
void UViperGameUserSettings::SetStickInvertY(bool bInvert) {bStickInvertY = bInvert;}
bool UViperGameUserSettings::GetStickInvertY() const {return bStickInvertY;}

//AUDIO
void UViperGameUserSettings::SetMonoAudioEnabled(bool bEnabled) {bMonoAudioEnabled = bEnabled;}
bool UViperGameUserSettings::GetMonoAudioEnabled() const {return bMonoAudioEnabled;}
void UViperGameUserSettings::SetMainVolume(float Volume) {VolumeMain = Volume;}
float UViperGameUserSettings::GetMainVolume() const {return VolumeMain;}
void UViperGameUserSettings::SetMusicVolume(float Volume) {VolumeMusic = Volume;}
float UViperGameUserSettings::GetMusicVolume() const {return VolumeMusic;}

//GRAPHICS
void UViperGameUserSettings::SetCameraFOV(float NewCameraFOV) {CameraFOV = NewCameraFOV;}
float UViperGameUserSettings::GetCameraFOV() const { return CameraFOV; }
void UViperGameUserSettings::SetGeneralQuality(int32 NewGeneralQuality){GeneralQuality = NewGeneralQuality;}
int32 UViperGameUserSettings::GetGeneralQuality() const {return GeneralQuality;}
