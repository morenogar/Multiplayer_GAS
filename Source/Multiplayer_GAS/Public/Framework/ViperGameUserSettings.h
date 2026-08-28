// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ViperGameUserSettings.generated.h"

namespace ViperGraphicsSettings
{
	constexpr int32 LowQuality = 0;
	constexpr int32 MediumQuality = 1;
	constexpr int32 HighQuality = 2;
	constexpr int32 UltraQuality = 3;
	constexpr int32 DefaultQuality = 1;
}

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
	
	//INPUT
	
	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetMouseLookSensitivityX(float Sensitivity);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	float GetMouseLookSensitivityX() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetMouseLookSensitivityY(float Sensitivity);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	float GetMouseLookSensitivityY() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetMouseInvertX(bool bInvert);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	bool GetMouseInvertX() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetMouseInvertY(bool bInvert);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	bool GetMouseInvertY() const;
	
	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetStickLookSensitivityX(float Sensitivity);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	float GetStickLookSensitivityX() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetStickLookSensitivityY(float Sensitivity);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	float GetStickLookSensitivityY() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetStickInvertX(bool bInvert);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	bool GetStickInvertX() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	void SetStickInvertY(bool bInvert);

	UFUNCTION(BlueprintCallable, Category="Settings|Input")
	bool GetStickInvertY() const;
	
	//AUDIO
	
	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	void SetMonoAudioEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	bool GetMonoAudioEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	void SetMainVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	float GetMainVolume() const;

	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	void SetMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="Settings|Audio")
	float GetMusicVolume() const;
	
	//GRAPHICS
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetCameraFOV(float NewCameraFOV);

	UFUNCTION(BlueprintCallable, Category=Settings)
	float GetCameraFOV() const;

	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetGeneralQuality(int32 NewGeneralQuality);

	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetGeneralQuality() const;
	
protected:
	
	//INPUT
	UPROPERTY(config)
	float MouseLookSensitivityX = 1.0f;

	UPROPERTY(config)
	float MouseLookSensitivityY = 1.0f;
	
	UPROPERTY(config)
	float StickLookSensitivityX = 1.0f;

	UPROPERTY(config)
	float StickLookSensitivityY = 1.0f;
	
	UPROPERTY(config)
	uint8 bMouseInvertX : 1 = 0;

	UPROPERTY(config)
	uint8 bMouseInvertY : 1 = 0;

	UPROPERTY(config)
	uint8 bStickInvertX : 1 = 0;

	UPROPERTY(config)
	uint8 bStickInvertY : 1 = 0;
	
	//AUDIO
	UPROPERTY(config)
	float VolumeMain = 1.0f;

	UPROPERTY(config)
	float VolumeMusic = 0.75f;
	
	UPROPERTY(config)
	uint8 bMonoAudioEnabled : 1 = 0;
	
	//GRAPHICS
	UPROPERTY(config)
	float CameraFOV = 90.0f;
	
	UPROPERTY(config)
	int32 GeneralQuality = ViperGraphicsSettings::DefaultQuality;
	
};
