// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"


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
