// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperBootUpSubsystem.h"

#include "OnlineSessionSettings.h"
#include "Framework/ViperSessionsSubsystem.h"
#include "OnlineSubsystem.h"

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
		ViperSessionsSubsystem->ViperOnSessionInviteAcceptedDelegate.AddUObject(this,&UViperBootUpSubsystem::OnSessionInviteAccepted);
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
	const bool bJoinedFromInvite = bTravelOnJoin;
	bTravelOnJoin = false;
	
	bool bResolved = false;
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
		{
			if (IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface())
			{
				bResolved = SessionInterface->GetResolvedConnectString(NAME_GameSession, LastValidAddress);
			}
		}
	}
	
	if (!bResolved)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to Join session")));
		CurrentSessionID = "";
		LastValidAddress = "";
		if (bJoinedFromInvite)
		{
			OnSteamInviteAccepted.Broadcast(false);
		}
		else
		{
			OnJoinCodeSessionFounded.Broadcast(false);
		}
		return;
	}
	
	// Steam invite: no UI step in between, travel straight to the host.
	if (bJoinedFromInvite)
	{
		OpenLobbyLevel();
		return;
	}
	
	OnJoinCodeSessionFounded.Broadcast(true);
}

void UViperBootUpSubsystem::OnSessionInviteAccepted(bool bWasSuccessful, const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful)
	{
		bTravelOnJoin = true;
		CurrentSessionID = InviteResult.GetSessionIdStr().Right(5);
	}
	OnSteamInviteAccepted.Broadcast(bWasSuccessful);
}

bool UViperBootUpSubsystem::ShowSteamInviteOverlay()
{
	return ViperSessionsSubsystem && ViperSessionsSubsystem->ShowInviteUI();
}

bool UViperBootUpSubsystem::IsInSession() const
{
	return ViperSessionsSubsystem && ViperSessionsSubsystem->IsInSession();
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
