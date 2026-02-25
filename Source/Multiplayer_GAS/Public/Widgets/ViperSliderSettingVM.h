// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ViperSettingBaseVM.h"
#include "ViperSliderSettingVM.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperSliderSettingVM : public UViperSettingBaseVM
{
	GENERATED_BODY()
	
public:
	float GetSliderValue() const { return SliderValue; }
	void SetSliderValue(const float NewValue);

	float GetMinValue() const { return MinValue; }
	void SetMinValue(const float NewMinValue);

	float GetMaxValue() const { return MaxValue; }
	void SetMaxValue(const float NewMaxValue);

	float GetStepSize() const { return StepSize; }
	void SetStepSize(const float NewStepSize);

	float GetDefaultSliderValue() const { return DefaultSliderValue; }
	void SetDefaultSliderValue(const float NewDefaultSliderValue);

	virtual bool RevertToDefaults() override;

private:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter, meta=(AllowPrivateAccess=true))
	float SliderValue = 0.5f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	float MinValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	float MaxValue = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	float StepSize = 0.1f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	float DefaultSliderValue = 0.5f;
};
