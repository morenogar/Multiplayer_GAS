// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "EnhancedInputSubsystemInterface.h"
#include "ViperActivatableWidget.generated.h"

UENUM(BlueprintType)
enum class EViperWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	UViperActivatableWidget();
	
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	
protected:
	
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EViperWidgetInputMode InputConfig = EViperWidgetInputMode::Default;
	
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
	
	
	UPROPERTY(EditDefaultsOnly, Category=Sound)
	FSlateSound ActivatedSound;

	UPROPERTY(EditDefaultsOnly, Category=Sound)
	FSlateSound DeactivatedSound;
	
	UPROPERTY(EditAnywhere, Category="Input", meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	FModifyContextOptions InputMappingOptions;
	
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	virtual void ActivateMappingContext() override;

};
