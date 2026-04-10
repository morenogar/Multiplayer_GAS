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
	
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnJoinCodeSessionFounded OnJoinCodeSessionFounded;
	
protected:
	
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful, FString SessionID);
	
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful, FString LastSessionCode);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	
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
};
