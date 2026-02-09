// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperActivatableWidget.h"
#include "CommonUITypes.h"
#include "EnhancedInputSubsystems.h"

UViperActivatableWidget::UViperActivatableWidget()
{
	InputMappingPriority = 12;
}


TOptional<FUIInputConfig> UViperActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case EViperWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EViperWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EViperWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EViperWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UViperActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UViperActivatableWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void UViperActivatableWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UViperActivatableWidget::ActivateMappingContext()
{
	if (CommonUI::IsEnhancedInputSupportEnabled() && InputMapping)
	{
		if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSystem->AddMappingContext(InputMapping, InputMappingPriority, InputMappingOptions);
			}
		}
	}
}