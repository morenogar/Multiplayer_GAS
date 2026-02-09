// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperActivatableWidgetStack.h"

#include "Widgets/ViperPopupBase.h"

UViperActivatableWidgetStack::UViperActivatableWidgetStack()
	: bContentAdded(false)
{
	
}


int32 UViperActivatableWidgetStack::GetStackCount(bool bIncludeRootContentWidgetIfValid) const
{
	return bIncludeRootContentWidgetIfValid ? (WidgetList.Num() + (GetRootContent() == nullptr ? 0 : 1)) : WidgetList.Num(); 
}

bool UViperActivatableWidgetStack::IsEmpty(bool bIncludeRootContentWidgetIfValid) const
{
	return WidgetList.IsEmpty() ? (bIncludeRootContentWidgetIfValid ? GetRootContent() == nullptr : true) : false;
}

UCommonActivatableWidget* UViperActivatableWidgetStack::TryGetTopOfStackWidget(bool bIncludeRootContentWidgetIfValid) const
{
	if (WidgetList.IsEmpty())
		return bIncludeRootContentWidgetIfValid ? GetRootContent() : nullptr;

	return WidgetList.Last();
}

void UViperActivatableWidgetStack::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	OnDisplayedWidgetChanged().AddUObject(this, &UViperActivatableWidgetStack::OnParentChangedDisplayedWidget);
}

void UViperActivatableWidgetStack::OnWidgetAddedToList(UCommonActivatableWidget& AddedWidget)
{
	Super::OnWidgetAddedToList(AddedWidget);

	if (GetRootContent() == nullptr || GetRootContent() != &AddedWidget)
	{
		bContentAdded = true;

		if (UViperPopupBase* popup = Cast<UViperPopupBase>(&AddedWidget))
		{
			popup->OnPopupClosed.AddUniqueDynamic(this, &UViperActivatableWidgetStack::OnPopupClosed);
		}
	}
}

void UViperActivatableWidgetStack::OnParentChangedDisplayedWidget(UCommonActivatableWidget* newWidget)
{
	//OnParentChangedDisplayedWidget is called on construct, so we need to make sure we're coming from having previously added content
	if ((newWidget == nullptr || newWidget == GetRootContent()) && bContentAdded)
	{
		bContentAdded = false;
		
		if (OnStackAtRootOrEmpty.IsBound())
			OnStackAtRootOrEmpty.Broadcast(this);
	}
}

void UViperActivatableWidgetStack::OnPopupClosed(UViperPopupBase* closedPopup)
{
	if (closedPopup != nullptr)
		RemoveWidget(*closedPopup);
}
