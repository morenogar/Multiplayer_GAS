// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperBootUpSubsystem.h"

#include "OnlineSessionSettings.h"
#include "Framework/ViperSessionsSubsystem.h"

void UViperBootUpSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		ViperSessionsSubsystem = GameInstance->GetSubsystem<UViperSessionsSubsystem>();
	}
	
	if (ViperSessionsSubsystem)
	{
		ViperSessionsSubsystem->ViperOnCreateSessionCompleteDelegate.AddDynamic(this,&UViperBootUpSubsystem::OnCreateSession);
		ViperSessionsSubsystem->ViperOnFindSessionsCompleteDelegate.AddUObject(this,&UViperBootUpSubsystem::OnFindSessions);
		ViperSessionsSubsystem->ViperOnJoinSessionCompleteDelegate.AddUObject(this,&UViperBootUpSubsystem::OnJoinSession);
		ViperSessionsSubsystem->ViperOnDestroySessionCompleteDelegate.AddDynamic(this,&UViperBootUpSubsystem::OnDestroySession);
		ViperSessionsSubsystem->ViperOnStartSessionCompleteDelegate.AddDynamic(this,&UViperBootUpSubsystem::OnStartSession);
	}
}

bool UViperBootUpSubsystem::TryToCreateSession()
{
	if (ViperSessionsSubsystem) ViperSessionsSubsystem->CreateSession(NumOfPublicConnections,FString(MatchType));
	return true;
}

bool UViperBootUpSubsystem::TryToSearchSessions()
{
	if (ViperSessionsSubsystem) ViperSessionsSubsystem->FindSession(10000);
	return true;
}

bool UViperBootUpSubsystem::TryToSearchSessionsByCode(FString ID)
{
	if (ViperSessionsSubsystem) ViperSessionsSubsystem->FindSessionByCode(ID,10000);
	return true;
}

void UViperBootUpSubsystem::UpdateMatchType(FString TypeOfMatch)
{
	MatchType = TypeOfMatch;
}

void UViperBootUpSubsystem::UpdateNumberOfPublicConnections(int32 NumberOfPublicConnections)
{
	NumOfPublicConnections = NumberOfPublicConnections;
}

void UViperBootUpSubsystem::OnCreateSession(bool bWasSuccessful, FString SessionID)
{
	if (bWasSuccessful)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, FString::Printf(TEXT("Created session complete!")));
		
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(FString("/Game/Maps/Lobby?listen"));
			CurrentSessionID = SessionID.Right(5);
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to create session")));
	}
}

void UViperBootUpSubsystem::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful, FString LastSessionCode)
{
	if (!ViperSessionsSubsystem) return;
	
	for (FOnlineSessionSearchResult Result : SessionSearchResults)
	{
		FString ID = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		FString SettingsValue;
		
		Result.Session.SessionSettings.Get(FName("MatchType"),SettingsValue);
		
		if (LastSessionCode.IsEmpty())
		{
			if (SettingsValue == MatchType)
			{
				ViperSessionsSubsystem->JoinSession(Result);
				return;
			}
		}
		
		if (LastSessionCode == ID.Right(5))
		{
			CurrentSessionID = ID.Right(5);
			ViperSessionsSubsystem->JoinSession(Result);
			return;
		}
	}
	
	if (!bWasSuccessful || SessionSearchResults.Num() == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to find sessions")));
		CurrentSessionID = "";
	}
	OnJoinCodeSessionFounded.Broadcast(false);
}

void UViperBootUpSubsystem::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface())
		{
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, LastValidAddress))
			{
					OnJoinCodeSessionFounded.Broadcast(true);
			}
		}
	}
	
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to Join session")));
		OnJoinCodeSessionFounded.Broadcast(false);
		CurrentSessionID = "";
	}
}

void UViperBootUpSubsystem::OnDestroySession(bool bWasSuccessful)
{
}

void UViperBootUpSubsystem::OnStartSession(bool bWasSuccessful)
{
}

void UViperBootUpSubsystem::OpenLobbyLevel()
{
	if (LastValidAddress.IsEmpty()) return;
	
	if (APlayerController* PlayerController = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController())
	{
		PlayerController->ClientTravel(LastValidAddress,ETravelType::TRAVEL_Absolute);
		LastValidAddress="";
	}
}
