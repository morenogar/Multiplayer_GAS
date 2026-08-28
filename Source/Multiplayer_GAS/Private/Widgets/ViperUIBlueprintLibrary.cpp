// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperUIBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Widgets/ViperHUD.h"
#include "Widgets/ViperOverallUILayout.h"
#include "Widgets/ViperPopupProviderInterface.h"
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

TScriptInterface<IViperSettingManagerInterface> UViperUIBlueprintLibrary::SearchUpWidgetHierarchyForSettingWidgetManager(UWidget* Widget)
{
	return SearchUpSWidgetHierarchyForWidgetWithInterface<IViperSettingManagerInterface, UViperSettingManagerInterface>(Widget);
}

TScriptInterface<IViperPopupProviderInterface> UViperUIBlueprintLibrary::SearchUpWidgetHierarchyForPopupProvider(UWidget* Widget)
{
	return SearchUpSWidgetHierarchyForWidgetWithInterface<IViperPopupProviderInterface, UViperPopupProviderInterface>(Widget);
}

FViperUIButtonCallback UViperUIBlueprintLibrary::TryGetCallbackFromUIButtonData(const FViperUICallbackButtonData& InButtonData, bool& OutCallbackWasBound)
{
	OutCallbackWasBound = false;

	if (InButtonData.Callback.IsBound() == false)
		return FViperUIButtonCallback();

	OutCallbackWasBound = true;
	return InButtonData.Callback;
}

bool UViperUIBlueprintLibrary::TryAssignCallbackToButtonData(const FViperUIButtonData& InButtonData,
	const FViperUIButtonCallback& Callback, FViperUICallbackButtonData& OutButtonData)
{
	if (Callback.IsBound() == false)
		return false;

	OutButtonData.ButtonData = InButtonData;
	OutButtonData.Callback = Callback;

	return true;
}
