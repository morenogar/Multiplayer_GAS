// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <Components/Widget.h>

#include "ViperPopupProviderInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Slate/SObjectWidget.h"
#include "ViperUIBlueprintLibrary.generated.h"

class IViperPopupProviderInterface;
class IViperSettingManagerInterface;
class UCommonActivatableWidget;
class UWidget;

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget")
	static UCommonActivatableWidget* PushContentToLayer_ForPlayer(const APlayerController* PlayerController, UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerName, UPARAM(meta = (AllowAbstract = false)) TSubclassOf<UCommonActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget")
	static void PopContentFromLayer(UCommonActivatableWidget* ActivatableWidget);
	
	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "Widget"))
	static TScriptInterface<IViperSettingManagerInterface> SearchUpWidgetHierarchyForSettingWidgetManager(UWidget* widget);

	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "Widget"))
	static TScriptInterface<IViperPopupProviderInterface> SearchUpWidgetHierarchyForPopupProvider(UWidget* widget);

	UFUNCTION(BlueprintPure)
	static FViperUIButtonCallback TryGetCallbackFromUIButtonData(UPARAM(Ref)const FViperUICallbackButtonData& inButtonData, bool& outCallbackWasBound);
	
	UFUNCTION(BlueprintPure)
	static bool TryAssignCallbackToButtonData(UPARAM(Ref)const FViperUIButtonData& inButtonData, UPARAM(Ref)const FViperUIButtonCallback& callback, FViperUICallbackButtonData& outButtonData);


private:
	template <typename IInterfaceType, typename UInterfaceType>
	static TScriptInterface<IInterfaceType> SearchUpSWidgetHierarchyForWidgetWithInterface(UWidget* InWidget)
	{
		if (InWidget == nullptr)
			return TScriptInterface<IInterfaceType>();

		if (InWidget->Implements<UInterfaceType>())
		{
			return TScriptInterface<IInterfaceType>(InWidget);
		}
		
		TSharedPtr<SWidget> widgetWalker = InWidget->GetCachedWidget();
		
		while (widgetWalker)
		{
			if (widgetWalker->GetType().IsEqual(TEXT("SObjectWidget")))
			{
				if (UUserWidget* userWidget = StaticCastSharedPtr<SObjectWidget>(widgetWalker)->GetWidgetObject())
				{
					if (userWidget->Implements<UInterfaceType>())
					{
						TScriptInterface<IInterfaceType> newScriptInterface(userWidget);
						return newScriptInterface;
					}
				}
			}
			widgetWalker = widgetWalker->GetParentWidget();
		}
		return TScriptInterface<IInterfaceType>();
	}
};
