// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "ViperSlider.generated.h"

class SViperSlider;
enum class ECommonInputType : uint8;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperSlider : public USlider
{
	GENERATED_BODY()
	
public:
	UViperSlider(const FObjectInitializer& ObjectInitializer);
	
	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnFloatValueChangedEvent OnAnalogCapture;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float StepTime = 0.1f;

protected:
	
	UFUNCTION(BlueprintCallable)
	void UpdateStepTime(float InValue) const;
	
	//Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//End UVisual Interface

	//Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//End UWidget Interface
	
	void HandleOnAnalogCapture(float InValue);

	void HandleInputMethodChanged(ECommonInputType CurrentInputType);
	TSharedPtr<SViperSlider> ViperSlider;
};
