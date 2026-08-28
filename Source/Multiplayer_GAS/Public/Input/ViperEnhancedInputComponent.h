// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "ViperInputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "ViperEnhancedInputComponent.generated.h"

/**
 * 
 */
UCLASS(config = Input)
class MULTIPLAYER_GAS_API UViperEnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:
	UViperEnhancedInputComponent(const FObjectInitializer& ObjectInitializer);

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UViperInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles);

};


template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UViperEnhancedInputComponent::BindAbilityActions(const UViperInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles)
{
	if (InputConfig)
	{
		for (const FViperInputAction& Action : InputConfig->AbilityInputActions)
		{
			if (Action.InputAction && Action.InputTag.IsValid())
			{
				if (PressedFunc)
				{
					BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, PressedFunc, Action.InputTag).GetHandle());
				}
				if (ReleasedFunc)
				{
					BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
				}
			}
		}
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("Attempting to bind actions on a null Input Config."));
	}
}
