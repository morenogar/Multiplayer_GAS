// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/ViperEnhancedInputComponent.h"


UViperEnhancedInputComponent::UViperEnhancedInputComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UViperEnhancedInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
