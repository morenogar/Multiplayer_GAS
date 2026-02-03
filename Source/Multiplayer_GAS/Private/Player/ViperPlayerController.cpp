// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperPlayerController.h"

#include "GAS/ViperAbilitySystemComponent.h"
#include "Player/ViperCharacter.h"
#include "Player/ViperPlayerCharacter.h"
#include "Widgets/ViperGameplayWidget.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_UILayerID_Gameplay, "UI.Layer.Gameplay");

AViperPlayerController::AViperPlayerController()
{

}

void AViperPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ViperPlayerCharacter = Cast<AViperPlayerCharacter>(InPawn);
	if(ViperPlayerCharacter)
	{
		ViperPlayerCharacter->ServerSideInit();
	}
}

void AViperPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	ViperPlayerCharacter = Cast<AViperPlayerCharacter>(InPawn);
	
	if(ViperPlayerCharacter)
	{
		ViperPlayerCharacter->ClientSideInit();
	}
}

void AViperPlayerController::SetGameplayWidget(UCommonActivatableWidget* Widget)
{
	if(!IsLocalPlayerController())
		return;
	
	if(UViperGameplayWidget* Gameplay = Cast<UViperGameplayWidget>(Widget))
		GameplayWidget = Gameplay;
}

void AViperPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);

	const AViperCharacter* ViperCharacter = Cast<AViperCharacter>(GetPawn());
	UViperAbilitySystemComponent*  AbilitySystem =  Cast<UViperAbilitySystemComponent>(ViperCharacter ? ViperCharacter->GetAbilitySystemComponent() : nullptr);

	if (AbilitySystem)
	{
		AbilitySystem->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
}

UViperAbilitySystemComponent* AViperPlayerController::GetViperAbilitySystemComponent() const
{
	const AViperPlayerCharacter* ViperCharacter = Cast<AViperPlayerCharacter>(GetPawn());
	return (ViperCharacter ? ViperCharacter->GetViperAbilitySystemComponent() : nullptr);
}
