// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/ViperSessionsSubsystem.h"

#include "OnlineSessionSettings.h"
//#include "OnlineReplStructs.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

UViperSessionsSubsystem::UViperSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnFindSessionComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnStartSessionComplete))
{
}

void UViperSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
	
	// Bind as early as possible: Steam keeps a +connect_lobby invite pending until someone listens to this delegate.
	if (SessionInterface.IsValid())
	{
		SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UViperSessionsSubsystem::OnSessionUserInviteAccepted));
	}
}

void UViperSessionsSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegateHandle);
		SessionInterface.Reset();
	}
	if (PendingInviteTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingInviteTickerHandle);
		PendingInviteTickerHandle.Reset();
	}
	PendingInviteResult.Reset();
	Super::Deinitialize();
}

void UViperSessionsSubsystem::CreateSession(int32 NumOfPublicConnections, FString MatchType)
{
	if (!SessionInterface.IsValid()) return;
	
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		bCreatedSessionOnDestroy = true;
		LastNumOfPublicConnections = NumOfPublicConnections;
		LastMatchType = MatchType;
		DestroySession();
	}
	
	 CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch =  IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumOfPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->bAllowInvites = true;
	LastSessionSettings->Set(FName("MatchType"),MatchType,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(),NAME_GameSession,*LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		ViperOnCreateSessionCompleteDelegate.Broadcast(false,"");
	}
}


void UViperSessionsSubsystem::FindSession(int32 MaxSearchResults)
{
	if (!SessionInterface.IsValid()) return;
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	// Find game sessions
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
	
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		ViperOnFindSessionsCompleteDelegate.Broadcast(TArray<FOnlineSessionSearchResult>(),false,LastSessionCode);
	}
}

void UViperSessionsSubsystem::FindSessionByCode(FString ID, int32 MaxSearchResults)
{
	LastSessionCode = ID;
	FindSession(MaxSearchResults);
}

void UViperSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionSearchResult)
{
	if (!SessionInterface.IsValid())
	{
		ViperOnJoinSessionCompleteDelegate.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		ViperOnJoinSessionCompleteDelegate.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(),NAME_GameSession,SessionSearchResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		ViperOnJoinSessionCompleteDelegate.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UViperSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		ViperOnDestroySessionCompleteDelegate.Broadcast(false);
		return;
	}
	
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		ViperOnDestroySessionCompleteDelegate.Broadcast(false);
	}
}

void UViperSessionsSubsystem::StartSession()
{
	
}

void UViperSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

	const FNamedOnlineSession* NamedSession =
		SessionInterface.IsValid() ? SessionInterface->GetNamedSession(SessionName) : nullptr;

	ViperOnCreateSessionCompleteDelegate.Broadcast(
		bWasSuccessful, NamedSession ? NamedSession->GetSessionIdStr() : TEXT(""));
}

void UViperSessionsSubsystem::OnFindSessionComplete(bool bWasSuccessful)
{
	if (SessionInterface)
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	
	if (LastSessionSearch->SearchResults.Num() == 0)
	{
		ViperOnFindSessionsCompleteDelegate.Broadcast(TArray<FOnlineSessionSearchResult>(),false,LastSessionCode);
		return;
	}
	
	ViperOnFindSessionsCompleteDelegate.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful,LastSessionCode);
}

void UViperSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	
	ViperOnJoinSessionCompleteDelegate.Broadcast(Result);
}

void UViperSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	
	if (bWasSuccessful && bCreatedSessionOnDestroy)
	{
		bCreatedSessionOnDestroy = false;
		CreateSession(LastNumOfPublicConnections,LastMatchType);
	}
	
	// We left our previous session to accept a Steam invite: now join the invited one.
	if (bJoinInviteOnDestroy)
	{
		bJoinInviteOnDestroy = false;
		if (bWasSuccessful)
		{
			JoinPendingInvite();
		}
		else
		{
			PendingInviteResult.Reset();
			ViperOnJoinSessionCompleteDelegate.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		}
	}
	ViperOnDestroySessionCompleteDelegate.Broadcast(bWasSuccessful);
}

void UViperSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}

// ---------------------------------------------------------------------------------------------------------------------
// Steam invites
// ---------------------------------------------------------------------------------------------------------------------

bool UViperSessionsSubsystem::IsInSession() const
{
	return SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

bool UViperSessionsSubsystem::ShowInviteUI()
{
	if (!IsInSession()) return false;
	
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	const IOnlineExternalUIPtr ExternalUI = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
	if (!ExternalUI.IsValid()) return false;
	
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return ExternalUI->ShowInviteUI(LocalPlayer ? LocalPlayer->GetControllerId() : 0, NAME_GameSession);
}

bool UViperSessionsSubsystem::SendInviteToFriend(const FUniqueNetId& FriendId)
{
	if (!IsInSession()) return false;
	
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return SessionInterface->SendSessionInviteToFriend(LocalPlayer ? LocalPlayer->GetControllerId() : 0, NAME_GameSession, FriendId);
}

void UViperSessionsSubsystem::OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ViperSessions] Steam invite accepted but the session could not be resolved."));
		ViperOnSessionInviteAcceptedDelegate.Broadcast(false, InviteResult);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ViperSessions] Steam invite accepted. Session: %s"), *InviteResult.GetSessionIdStr());
	
	// If the game was launched from the invite this arrives during boot, before the player controller exists.
	// Store it and process it from a ticker once the local player is ready.
	PendingInviteResult = InviteResult;
	if (!PendingInviteTickerHandle.IsValid())
	{
		PendingInviteTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UViperSessionsSubsystem::TickPendingInvite));
	}
}

bool UViperSessionsSubsystem::TickPendingInvite(float DeltaTime)
{
	if (!PendingInviteResult.IsSet())
	{
		PendingInviteTickerHandle.Reset();
		return false;
	}
	
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->PlayerController || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		return true; // Keep waiting
	}
	
	PendingInviteTickerHandle.Reset();
	ProcessPendingInvite();
	return false;
}

void UViperSessionsSubsystem::ProcessPendingInvite()
{
	if (!SessionInterface.IsValid() || !PendingInviteResult.IsSet()) return;
	
	const FOnlineSessionSearchResult& Invite = PendingInviteResult.GetValue();
	
	if (const FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession))
	{
		// Already in the invited session: nothing to do.
		if (CurrentSession->GetSessionIdStr() == Invite.GetSessionIdStr())
		{
			PendingInviteResult.Reset();
			return;
		}
		
		// Leave the current session first (as host this disconnects our clients), then join in OnDestroySessionComplete.
		ViperOnSessionInviteAcceptedDelegate.Broadcast(true, Invite);
		bJoinInviteOnDestroy = true;
		DestroySession();
		return;
	}
	
	ViperOnSessionInviteAcceptedDelegate.Broadcast(true, Invite);
	JoinPendingInvite();
}

void UViperSessionsSubsystem::JoinPendingInvite()
{
	if (!PendingInviteResult.IsSet()) return;
	
	const FOnlineSessionSearchResult Invite = PendingInviteResult.GetValue();
	PendingInviteResult.Reset();
	JoinSession(Invite);
}

const ULocalPlayer* UViperSessionsSubsystem::GetLocalPlayer() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
}
