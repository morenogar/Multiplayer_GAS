// Fill out your copyright notice in the Description page of Project Settings.
#include "Widgets/ViperPopupProviderInterface.h"

void IViperPopupProviderInterface::DisplayConfirmCancelPopup_Implementation(const FPopupInfo& popupInfo, const FPopupChoiceCallback& confirmCallback, const FPopupChoiceCallback& cancelCallback, FViperConfirmCancelPopupOverrides overrides)
{
}

void IViperPopupProviderInterface::DisplayMultiOptionPopup_Implementation(const FPopupInfo& popupInfo, UPARAM(Ref)const TArray<FViperUICallbackButtonData>& options, const FPopupChoiceCallback& popupClosedCallback, int32 intialHoveredOption /*= -1*/)
{
}

UViperPopupBase* IViperPopupProviderInterface::GenerateAndDisplayCustomPopup_Implementation(TSubclassOf<UViperPopupBase> popupClass, const bool bIsVisualOnly /* = false */)
{
	return nullptr;
}
