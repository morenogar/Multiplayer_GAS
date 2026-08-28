// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ViperPopupBase.h"
#include "UObject/Interface.h"
#include "ViperPopupProviderInterface.generated.h"

USTRUCT(BlueprintType)
struct FViperUIButtonData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ButtonText = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UInputAction* ButtonAction = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 ButtonIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint32 bIsLocked : 1;

	FViperUIButtonData()
		: bIsLocked(false)
	{}
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FPopupChoiceCallback, UViperPopupBase*, Caller);
DECLARE_DYNAMIC_DELEGATE_OneParam(FViperUIButtonCallback, FViperUIButtonData, ButtonData);

USTRUCT(BlueprintType)
struct FViperUICallbackButtonData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FViperUIButtonData ButtonData = FViperUIButtonData();
	
	UPROPERTY()
	FViperUIButtonCallback Callback =  FViperUIButtonCallback();
};

USTRUCT(BlueprintType)
struct FViperConfirmCancelPopupOverrides
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ConfirmTextOverride = FText::GetEmpty();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText CancelTextOverride = FText::GetEmpty();
};

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UViperPopupProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperPopupProviderInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|PopupProviderInterface")
	void DisplayConfirmCancelPopup(const FPopupInfo& PopupInfo, const FPopupChoiceCallback& ConfirmCallback, const FPopupChoiceCallback& CancelCallback, FViperConfirmCancelPopupOverrides Overrides);
	virtual void DisplayConfirmCancelPopup_Implementation(const FPopupInfo& PopupInfo, const FPopupChoiceCallback& ConfirmCallback, const FPopupChoiceCallback& CancelCallback, FViperConfirmCancelPopupOverrides Overrides);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|PopupProviderInterface")
	void DisplayMultiOptionPopup(const FPopupInfo& PopupInfo, UPARAM(Ref)const TArray<FViperUICallbackButtonData>& Options, const FPopupChoiceCallback& PopupClosedCallback, int32 InitialHoveredOption = -1);
	virtual void DisplayMultiOptionPopup_Implementation(const FPopupInfo& PopupInfo, UPARAM(Ref)const TArray<FViperUICallbackButtonData>& Options, const FPopupChoiceCallback& PopupClosedCallback, int32 InitialHoveredOption = -1);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Viper|PopupProviderInterface")
	UViperPopupBase* GenerateAndDisplayCustomPopup(TSubclassOf<UViperPopupBase> PopupClass, const bool bIsVisualOnly = false);
	virtual UViperPopupBase* GenerateAndDisplayCustomPopup_Implementation(TSubclassOf<UViperPopupBase> PopupClass, const bool bIsVisualOnly = false);
};
