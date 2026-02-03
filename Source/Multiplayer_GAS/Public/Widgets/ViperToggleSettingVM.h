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
	bool GetToggleValue() const { return ToggleValue; };
	void SetToggleValue(const bool newToggleValue);

	bool GetDefaultToggleValue() const { return DefaultToggleValue; }
	void SetDefaultToggleValue(bool newDefaultToggleValue);

	virtual bool RevertToDefaults() override;

private:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter, meta=(AllowPrivateAccess=true))
	bool ToggleValue = false;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	bool DefaultToggleValue = false;
};
