// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ViperBootUpSubsystem.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinCodeSessionFounded, bool, bFounded);
/** bWasSuccessful = true when a Steam invite was accepted and we started joining; false if the invite or the join failed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamInviteAccepted, bool, bWasSuccessful);

UCLASS()
class MULTIPLAYER_GAS_API UViperBootUpSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	bool TryToCreateSession();
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	bool TryToSearchSessions();
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	bool TryToSearchSessionsByCode(FString ID);
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	void UpdateMatchType(FString TypeOfMatch = "FreeForAll");
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	void UpdateNumberOfPublicConnections(int32 NumberOfPublicConnections = 4);
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	void OpenLobbyLevel();
	
	UFUNCTION(BlueprintPure, Category = "Viper|BootUp")
	FString GetCurrentSessionID() {return CurrentSessionID; };
	
	/** Opens the Steam overlay to invite friends to the current session. Only works with Steam running (Standalone / packaged). */
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	bool ShowSteamInviteOverlay();
	
	UFUNCTION(BlueprintPure, Category = "Viper|BootUp")
	bool IsInSession() const;
	
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnJoinCodeSessionFounded OnJoinCodeSessionFounded;
	
	/** Use it to show a "Joining friend..." popup, or an error if it fails. Travel to the host is automatic. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnSteamInviteAccepted OnSteamInviteAccepted;
	
protected:
	
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful, FString SessionID);
	
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful, FString LastSessionCode);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	void OnSessionInviteAccepted(bool bWasSuccessful, const FOnlineSessionSearchResult& InviteResult);
	
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:
	
	class UViperSessionsSubsystem* ViperSessionsSubsystem;
	
	int32 NumOfPublicConnections = 4;
	FString MatchType = "FreeForAll";
	FString CurrentSessionID = "";
	
	FString LastValidAddress = "";
	
	/** True while joining a session from a Steam invite: travel automatically when the join completes. */
	bool bTravelOnJoin = false;
};
