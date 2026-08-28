// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperGameplayStatics.h"

#include "Player/ViperPlayerCharacter.h"
#include "Player/ViperPlayerController.h"


AViperPlayerController* UViperGameplayStatics::GetLocalPlayerController(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		APlayerController* Controller = GEngine->GetFirstLocalPlayerController(World);
		if (AViperPlayerController* ViperController = Cast<AViperPlayerController>(Controller))
		{
			return ViperController;
		}
	}
	
	return nullptr;
}

AViperPlayerCharacter* UViperGameplayStatics::GetLocalCharacter(const UObject* WorldContextObject)
{
	AViperPlayerController* LocalController = GetLocalPlayerController(WorldContextObject);
	return LocalController ? Cast<AViperPlayerCharacter>(LocalController->GetPawn()) : nullptr;
}
