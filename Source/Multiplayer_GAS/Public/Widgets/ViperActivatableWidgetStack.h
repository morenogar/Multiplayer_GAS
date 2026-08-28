// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "ViperActivatableWidgetStack.generated.h"

class UViperPopupBase;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperActivatableWidgetStack : public UCommonActivatableWidgetStack
{
	GENERATED_BODY()
public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAgoraPopupStackEvent, UViperActivatableWidgetStack*, caller);

	UViperActivatableWidgetStack();
	
	UPROPERTY(BlueprintAssignable, Category = "Agora|PopupStackRoot")
	FAgoraPopupStackEvent OnStackAtRootOrEmpty;

	UFUNCTION(BlueprintPure)
	int32 GetStackCount(bool bIncludeRootContentWidgetIfValid = false) const;

	UFUNCTION(BlueprintPure)
	bool IsEmpty(bool bIncludeRootContentWidgetIfValid = false) const;

	UFUNCTION(BlueprintPure)
	UCommonActivatableWidget* TryGetTopOfStackWidget(bool bIncludeRootContentWidgetIfValid = false) const;

	
protected:
	virtual void SynchronizeProperties() override;
	virtual void OnWidgetAddedToList(UCommonActivatableWidget& AddedWidget) override;
	
private:
	void OnParentChangedDisplayedWidget(UCommonActivatableWidget* NewWidget);
	UFUNCTION()
	void OnPopupClosed(UViperPopupBase* ClosedPopup);
	
	uint32 bContentAdded : 1; //Defaults to false in initializer list
};
