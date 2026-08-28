// Fill out your copyright notice in the Description page of Project Settings.
#include "Widgets/ViperPopupProviderInterface.h"

void IViperPopupProviderInterface::DisplayConfirmCancelPopup_Implementation(const FPopupInfo& PopupInfo, const FPopupChoiceCallback& ConfirmCallback, const FPopupChoiceCallback& CancelCallback, FViperConfirmCancelPopupOverrides Overrides)
{
}

void IViperPopupProviderInterface::DisplayMultiOptionPopup_Implementation(const FPopupInfo& PopupInfo, UPARAM(Ref)const TArray<FViperUICallbackButtonData>& Options, const FPopupChoiceCallback& PopupClosedCallback, int32 InitialHoveredOption /*= -1*/)
{
}

UViperPopupBase* IViperPopupProviderInterface::GenerateAndDisplayCustomPopup_Implementation(TSubclassOf<UViperPopupBase> PopupClass, const bool bIsVisualOnly /* = false */)
{
	return nullptr;
}
