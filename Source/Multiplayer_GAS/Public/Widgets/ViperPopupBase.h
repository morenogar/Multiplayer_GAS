// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ViperActivatableWidget.h"
#include "ViperPopupBase.generated.h"


USTRUCT(BlueprintType)
struct FPopupInfo
{
	GENERATED_BODY()

	FPopupInfo()
	{}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viper|PopupInfo")
	FText Title = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viper|PopupInfo")
	FText Body = FText::GetEmpty();
};

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperPopupBase : public UViperActivatableWidget
{
	GENERATED_BODY()
		
public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPopupEvent, UViperPopupBase*, caller);

	UViperPopupBase();
	
	UFUNCTION(BlueprintCallable)
	void ClosePopup();
	
	UPROPERTY(BlueprintAssignable)
	FPopupEvent OnPopupClosed;

protected:
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintProtected))
	void ParentSetup(const FPopupInfo& popupEventData);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	uint32 bCloseOnOptionChosen : 1; 
	
	UPROPERTY(EditDefaultsOnly)
	uint32 bUnbindCloseBindingsOnClose : 1;
};
