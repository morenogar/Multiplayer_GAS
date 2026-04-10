// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperInteractResponse.h"
#include "CommonActivatableWidget.h"
#include "Widgets/ViperHUD.h"


void UViperInteractResponse::Interact(APlayerController* player) const
{
	switch (Type)
	{
	case ETargetingInteractResponseType::OpenMenu:
		{
			AViperHUD* hud = player->GetHUD<AViperHUD>();
			if (UViperOverallUILayout* overallUILayout = hud ? hud->GetOverallUILayout() : nullptr)
			{
				if (UCommonActivatableWidget* activatableWidget = overallUILayout->PushWidgetToLayerStack(MenuUILayer, MenuToOpen))
				{
					activatableWidget->OnDeactivated().AddUObject(this, &UViperInteractResponse::MenuClosed);
					activatableWidget->SetOwningPlayer(player);
					
				}
			}
		}
		break;
	default: ;
	}
}

void UViperInteractResponse::MenuClosed() const
{
	if (InteractResponseWidget == nullptr) return;
	if (APlayerController* PlayerController = InteractResponseWidget->GetOwningPlayer())
	{
		AViperHUD* hud = PlayerController->GetHUD<AViperHUD>();
		if (UViperOverallUILayout* overallUILayout = hud ? hud->GetOverallUILayout() : nullptr)
		{
			overallUILayout->FindAndRemoveWidgetFromLayer(InteractResponseWidget);
		}
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(false);
	}
}
