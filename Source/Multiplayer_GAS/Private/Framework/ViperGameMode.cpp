// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

AViperGameMode::AViperGameMode()
{
	// Clients travel together with the host instead of disconnecting and reconnecting on map change.
	bUseSeamlessTravel = true;
}


void AViperGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (GameState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();
		
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("Numbers of players %d"),NumberOfPlayers));
		
		if (APlayerState* PlayerState = NewPlayer->GetPlayerState<APlayerState>())
		{
			FString PlayerName = PlayerState->GetPlayerName();
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("%s has joined the game"),*PlayerName));
		}
	}
}

void AViperGameMode::Logout(AController* ExitingController)
{
	Super::Logout(ExitingController);
	
	if (GameState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num() - 1;
		
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("Numbers of players %d"),NumberOfPlayers));
		
		if (APlayerState* PlayerState = ExitingController->GetPlayerState<APlayerState>())
		{
			FString PlayerName = PlayerState->GetPlayerName();
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("%s has left the game"),*PlayerName));
		}
	}
}

bool AViperGameMode::StartGame(TSoftObjectPtr<UWorld> Map)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return false;
	}
	
	if (Map.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ViperGameMode] StartGame: no map selected."));
		return false;
	}
	
	if (World->IsInSeamlessTravel())
	{
		return false;
	}
	
	// ?listen keeps the host as a listen server in the new map. Every client follows via ClientTravel.
	const FString TravelURL = Map.ToSoftObjectPath().GetLongPackageName() + TEXT("?listen");
	UE_LOG(LogTemp, Log, TEXT("[ViperGameMode] StartGame: server travel to %s"), *TravelURL);
	return World->ServerTravel(TravelURL);
}
