// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperGameplayStatics.h"

#include "Player/ViperPlayerCharacter.h"
#include "Player/ViperPlayerController.h"


AViperPlayerController* UViperGameplayStatics::GetLocalPlayerController(const UObject* worldContextObject)
{
	if (!IsValid(worldContextObject))
	{
		return nullptr;
	}
	
	if (UWorld* World = GEngine->GetWorldFromContextObject(worldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		APlayerController* Controller = GEngine->GetFirstLocalPlayerController(World);
		if (AViperPlayerController* ViperController = Cast<AViperPlayerController>(Controller))
		{
			return ViperController;
		}
	}
	
	return nullptr;
}

AViperPlayerCharacter* UViperGameplayStatics::GetLocalCharacter(const UObject* worldContextObject)
{
	AViperPlayerController* localController = GetLocalPlayerController(worldContextObject);
	return localController ? Cast<AViperPlayerCharacter>(localController->GetPawn()) : nullptr;
}
