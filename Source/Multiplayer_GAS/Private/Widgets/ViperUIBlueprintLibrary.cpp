// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperUIBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Widgets/ViperHUD.h"
#include "Widgets/ViperOverallUILayout.h"
#include "Widgets/ViperSettingManagerInterface.h"


UCommonActivatableWidget* UViperUIBlueprintLibrary::PushContentToLayer_ForPlayer(
	const APlayerController* PlayerController, FGameplayTag LayerName,
	TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!ensure(PlayerController) || !ensure(WidgetClass != nullptr))
	{
		return nullptr;	
	}
	AHUD* HUD = PlayerController ? PlayerController->GetHUD() : nullptr;
	AViperHUD* ViperHUD = HUD ? Cast<AViperHUD>(HUD) : nullptr;
	if (UViperOverallUILayout* OverallUILayout = ViperHUD ? ViperHUD->GetOverallUILayout() : nullptr)
	{
		return OverallUILayout->PushWidgetToLayerStack(LayerName, WidgetClass);
	}

	return nullptr;
}

void UViperUIBlueprintLibrary::PopContentFromLayer(UCommonActivatableWidget* ActivatableWidget)
{
	if (IsValid(ActivatableWidget))
	{
		const ULocalPlayer* LocalPlayer = ActivatableWidget->GetOwningLocalPlayer();
		const APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
		AViperHUD* ViperHUD = PlayerController ? Cast<AViperHUD>(PlayerController->GetHUD()) : nullptr;
		if (UViperOverallUILayout* OverallUILayout = ViperHUD ? ViperHUD->GetOverallUILayout() : nullptr)
		{
			OverallUILayout->FindAndRemoveWidgetFromLayer(ActivatableWidget);
		}
	}
}

TScriptInterface<IViperSettingManagerInterface> UViperUIBlueprintLibrary::SearchUpWidgetHierarchyForSettingWidgetManager(UWidget* widget)
{
	return SearchUpSWidgetHierarchyForWidgetWithInterface<IViperSettingManagerInterface, UViperSettingManagerInterface>(widget);
}



