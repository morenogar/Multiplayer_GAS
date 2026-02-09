// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperPopupBase.h"

UViperPopupBase::UViperPopupBase()
	: bCloseOnOptionChosen(true)
	, bUnbindCloseBindingsOnClose(true)
{
}

void UViperPopupBase::ClosePopup()
{
	DeactivateWidget();
	
	if (OnPopupClosed.IsBound())
	{
		if (bUnbindCloseBindingsOnClose)
		{
			//Make a copy so we can unbind before broadcasting
			FPopupEvent tempCloseEvent = OnPopupClosed;
			OnPopupClosed.Clear();
			tempCloseEvent.Broadcast(this);
		}
		else
		{
			OnPopupClosed.Broadcast(this);
		}
	}
}

void UViperPopupBase::ParentSetup(const FPopupInfo& popupEventData)
{

}