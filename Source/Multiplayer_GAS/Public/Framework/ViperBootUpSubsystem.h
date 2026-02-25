// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ViperBootUpSubsystem.generated.h"

/**
 * 
 */
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
	void UpdateMatchType(FString TypeOfMatch = "FreeForAll");
	
	UFUNCTION(BlueprintCallable, Category = "Viper|BootUp")
	void UpdateNumberOfPublicConnections(int32 NumberOfPublicConnections = 4);
	
protected:
	
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);
	
private:
	
	class UViperSessionsSubsystem* ViperSessionsSubsystem;
	
	int32 NumOfPublicConnections = 4;
	FString MatchType = "FreeForAll";
	
};
