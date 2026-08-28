// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ViperSettingBaseVM.h"
#include "ViperToggleSettingVM.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperToggleSettingVM : public UViperSettingBaseVM
{
	GENERATED_BODY()
	
public:
	bool GetToggleValue() const { return bToggleValue; };
	void SetToggleValue(const bool NewToggleValue);

	bool GetDefaultToggleValue() const { return bDefaultToggleValue; }
	void SetDefaultToggleValue(bool NewDefaultToggleValue);

	virtual bool RevertToDefaults() override;

private:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter=GetToggleValue, Setter=SetToggleValue, meta=(AllowPrivateAccess=true))
	bool bToggleValue = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter=GetDefaultToggleValue, meta=(AllowPrivateAccess=true))
	bool bDefaultToggleValue = false;
};
