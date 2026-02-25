// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperSlider.h"
#include "Widgets/SViperSlider.h"
#include "CommonInputSubsystem.h"

UViperSlider::UViperSlider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UViperSlider::UpdateStepTime(float InValue) const
{
	if (ViperSlider != nullptr)
	{
		ViperSlider->SetStepTime(InValue);
	}
}

void UViperSlider::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	ViperSlider.Reset();
	
	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
	}
}

void UViperSlider::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

TSharedRef<SWidget> UViperSlider::RebuildWidget()
{
	MySlider = ViperSlider = SNew(SViperSlider)
	.Style(&GetWidgetStyle())
	.IsFocusable(IsFocusable)
	.OnMouseCaptureBegin(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnMouseCaptureBegin))
	.OnMouseCaptureEnd(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnMouseCaptureEnd))
	.OnControllerCaptureBegin(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnControllerCaptureBegin))
	.OnControllerCaptureEnd(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnControllerCaptureEnd))
	.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged))
	.OnAnalogCapture(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnAnalogCapture))
	.StepTime(StepTime);
	
	if (UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		ViperSlider->SetUsingGamepad(CommonInputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad);
		CommonInputSubsystem->OnInputMethodChangedNative.AddUObject(this, &UViperSlider::HandleInputMethodChanged);
	}

	return ViperSlider.ToSharedRef();
}

void UViperSlider::HandleOnAnalogCapture(float InValue)
{
	OnAnalogCapture.Broadcast(InValue);
}

void UViperSlider::HandleInputMethodChanged(ECommonInputType CurrentInputType)
{
	if (ViperSlider != nullptr)
	{
		ViperSlider->SetUsingGamepad(CurrentInputType == ECommonInputType::Gamepad);
	}
}
