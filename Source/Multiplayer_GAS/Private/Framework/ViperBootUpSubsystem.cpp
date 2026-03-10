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

void UViperBootUpSubsystem::UpdateMatchType(FString TypeOfMatch)
{
	MatchType = TypeOfMatch;
}

void UViperBootUpSubsystem::UpdateNumberOfPublicConnections(int32 NumberOfPublicConnections)
{
	NumOfPublicConnections = NumberOfPublicConnections;
}

void UViperBootUpSubsystem::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, FString::Printf(TEXT("Created session complete!")));
		
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(FString("/Game/Maps/Lobby?listen"));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to create session")));
	}
}

void UViperBootUpSubsystem::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful)
{
	if (!ViperSessionsSubsystem) return;
	
	for (FOnlineSessionSearchResult Result : SessionSearchResults)
	{
		FString ID = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		FString SettingsValue;
		
		Result.Session.SessionSettings.Get(FName("MatchType"),SettingsValue);
		
		if (SettingsValue == MatchType)
		{
			ViperSessionsSubsystem->JoinSession(Result);
			return;
		}
	}
	
	if (!bWasSuccessful || SessionSearchResults.Num() == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to find sessions")));
	}
}

void UViperBootUpSubsystem::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface())
		{
			FString Address;
	
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				if (APlayerController* PlayerController = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController())
				{
					PlayerController->ClientTravel(Address,ETravelType::TRAVEL_Absolute);
				}
			}
		}
	}
	
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to Join session")));
	}
}

void UViperBootUpSubsystem::OnDestroySession(bool bWasSuccessful)
{
}

void UViperBootUpSubsystem::OnStartSession(bool bWasSuccessful)
{
}
