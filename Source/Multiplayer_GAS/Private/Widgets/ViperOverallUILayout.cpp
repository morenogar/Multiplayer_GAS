// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperOverallUILayout.h"

void UViperOverallUILayout::FindAndRemoveWidgetFromLayer(UCommonActivatableWidget* ActivatableWidget)
{
	for (const auto& LayerKVP : Layers)
	{
		LayerKVP.Value->RemoveWidget(*ActivatableWidget);
	}
}

void UViperOverallUILayout::ClearAllWidgets()
{
	for (const auto& LayerKVP : Layers)
	{
		LayerKVP.Value->ClearWidgets();
	}
}

UCommonActivatableWidgetContainerBase* UViperOverallUILayout::GetLayerWidget(FGameplayTag LayerName)
{
	return Layers.FindRef(LayerName);
}

void UViperOverallUILayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget)
{
	if (!IsDesignTime())
	{
		LayerWidget->SetTransitionDuration(0.0);
		Layers.Add(LayerTag, LayerWidget);
	}
}



