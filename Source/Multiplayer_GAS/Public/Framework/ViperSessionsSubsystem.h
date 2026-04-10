// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "ViperSessionsSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FViperOnCreateSessionCompleteDelegate, bool, bWasSuccessful, FString, SessionCode);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FViperOnFindSessionsCompleteDelegate, const TArray<FOnlineSessionSearchResult>& SessionSearch, bool bWasSuccessful, FString SessionCode);
DECLARE_MULTICAST_DELEGATE_OneParam(FViperOnJoinSessionCompleteDelegate, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FViperOnDestroySessionCompleteDelegate,bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FViperOnStartSessionComplete,bool, bWasSuccessful);
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	
	UViperSessionsSubsystem();
	
	void CreateSession(int32 NumOfPublicConnections,FString MatchType);
	void FindSession(int32 MaxSearchResults);
	void FindSessionByCode(FString ID, int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionSearchResult);
	void DestroySession();
	void StartSession();
	
	FViperOnCreateSessionCompleteDelegate ViperOnCreateSessionCompleteDelegate;
	FViperOnDestroySessionCompleteDelegate ViperOnDestroySessionCompleteDelegate;
	FViperOnFindSessionsCompleteDelegate ViperOnFindSessionsCompleteDelegate;
	FViperOnJoinSessionCompleteDelegate ViperOnJoinSessionCompleteDelegate;
	FViperOnStartSessionComplete ViperOnStartSessionCompleteDelegate;
	
protected:
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnFindSessionComplete(bool bWasSuccessful);
	
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FNamedOnlineSession> LastNamedSession;
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
	
	bool bCreatedSessionOnDestroy = false;
	int32 LastNumOfPublicConnections;
	FString LastMatchType;
	FString LastSessionCode;
};
