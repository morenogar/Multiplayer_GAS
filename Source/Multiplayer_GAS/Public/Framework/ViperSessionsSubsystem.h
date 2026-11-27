// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Containers/Ticker.h"
#include "OnlineSessionSettings.h"
class ULocalPlayer;

#include "ViperSessionsSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FViperOnCreateSessionCompleteDelegate, bool, bWasSuccessful, FString, SessionCode);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FViperOnFindSessionsCompleteDelegate, const TArray<FOnlineSessionSearchResult>& SessionSearch, bool bWasSuccessful, FString SessionCode);
DECLARE_MULTICAST_DELEGATE_OneParam(FViperOnJoinSessionCompleteDelegate, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FViperOnDestroySessionCompleteDelegate,bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FViperOnStartSessionComplete,bool, bWasSuccessful);
/** Fired when the local player accepted a Steam invite and we are about to join that session. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FViperOnSessionInviteAcceptedDelegate, bool /*bWasSuccessful*/, const FOnlineSessionSearchResult& /*InviteResult*/);
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	
	UViperSessionsSubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void CreateSession(int32 NumOfPublicConnections,FString MatchType);
	void FindSession(int32 MaxSearchResults);
	void FindSessionByCode(FString ID, int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionSearchResult);
	void DestroySession();
	void StartSession();
	
	/** Opens the Steam overlay invite dialog for the current session (lobby). Returns false if there is no session. */
	bool ShowInviteUI();
	/** Sends a Steam invite to a specific friend for the current session (for an in-game friends list). */
	bool SendInviteToFriend(const FUniqueNetId& FriendId);
	bool IsInSession() const;
	
	FViperOnCreateSessionCompleteDelegate ViperOnCreateSessionCompleteDelegate;
	FViperOnDestroySessionCompleteDelegate ViperOnDestroySessionCompleteDelegate;
	FViperOnFindSessionsCompleteDelegate ViperOnFindSessionsCompleteDelegate;
	FViperOnJoinSessionCompleteDelegate ViperOnJoinSessionCompleteDelegate;
	FViperOnStartSessionComplete ViperOnStartSessionCompleteDelegate;
	FViperOnSessionInviteAcceptedDelegate ViperOnSessionInviteAcceptedDelegate;
	
protected:
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnFindSessionComplete(bool bWasSuccessful);
	
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	
	/** Steam: invite accepted from the overlay, or the game was launched with +connect_lobby. */
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

private:
	
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
	
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate; 
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;
	
	// Pending Steam invite: processed once a local player/controller exists, and after leaving the current session if needed.
	bool TickPendingInvite(float DeltaTime);
	void ProcessPendingInvite();
	void JoinPendingInvite();
	const ULocalPlayer* GetLocalPlayer() const;
	
	TOptional<FOnlineSessionSearchResult> PendingInviteResult;
	FTSTicker::FDelegateHandle PendingInviteTickerHandle;
	bool bJoinInviteOnDestroy = false;
	
	bool bCreatedSessionOnDestroy = false;
	int32 LastNumOfPublicConnections;
	FString LastMatchType;
	FString LastSessionCode;
};
