// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperInteractResponse.h"
#include "CommonActivatableWidget.h"
#include "Widgets/ViperHUD.h"


void UViperInteractResponse::Interact(APlayerController* Player)
{
	switch (Type)
	{
	case ETargetingInteractResponseType::OpenMenu:
		{
			AViperHUD* HUD = Player->GetHUD<AViperHUD>();
			if (UViperOverallUILayout* OverallUILayout = HUD ? HUD->GetOverallUILayout() : nullptr)
			{
				if (UCommonActivatableWidget* ActivatableWidget = OverallUILayout->PushWidgetToLayerStack(MenuUILayer, MenuToOpen))
				{
					FInputModeUIOnly InputMode;
					InteractResponseWidget = ActivatableWidget;
					Player->SetInputMode(InputMode);
					Player->SetShowMouseCursor(false);
					ActivatableWidget->OnDeactivated().AddUObject(this, &UViperInteractResponse::MenuClosed);
					ActivatableWidget->SetOwningPlayer(Player);

				}
			}
		}
		break;
	default: ;
	}
}

void UViperInteractResponse::MenuClosed()
{
	if (InteractResponseWidget == nullptr) return;
	if (APlayerController* PlayerController = InteractResponseWidget->GetOwningPlayer())
	{
		AViperHUD* HUD = PlayerController->GetHUD<AViperHUD>();
		if (UViperOverallUILayout* OverallUILayout = HUD ? HUD->GetOverallUILayout() : nullptr)
		{
			OverallUILayout->FindAndRemoveWidgetFromLayer(InteractResponseWidget);
		}
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(false);
	}
}
