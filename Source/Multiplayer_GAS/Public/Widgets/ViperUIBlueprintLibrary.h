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
	static TScriptInterface<IViperSettingManagerInterface> SearchUpWidgetHierarchyForSettingWidgetManager(UWidget* Widget);

	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "Widget"))
	static TScriptInterface<IViperPopupProviderInterface> SearchUpWidgetHierarchyForPopupProvider(UWidget* Widget);

	UFUNCTION(BlueprintPure)
	static FViperUIButtonCallback TryGetCallbackFromUIButtonData(UPARAM(Ref)const FViperUICallbackButtonData& InButtonData, bool& OutCallbackWasBound);

	UFUNCTION(BlueprintPure)
	static bool TryAssignCallbackToButtonData(UPARAM(Ref)const FViperUIButtonData& InButtonData, UPARAM(Ref)const FViperUIButtonCallback& Callback, FViperUICallbackButtonData& OutButtonData);


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
		
		TSharedPtr<SWidget> WidgetWalker = InWidget->GetCachedWidget();

		while (WidgetWalker)
		{
			if (WidgetWalker->GetType().IsEqual(TEXT("SObjectWidget")))
			{
				if (UUserWidget* UserWidget = StaticCastSharedPtr<SObjectWidget>(WidgetWalker)->GetWidgetObject())
				{
					if (UserWidget->Implements<UInterfaceType>())
					{
						TScriptInterface<IInterfaceType> NewScriptInterface(UserWidget);
						return NewScriptInterface;
					}
				}
			}
			WidgetWalker = WidgetWalker->GetParentWidget();
		}
		return TScriptInterface<IInterfaceType>();
	}
};
