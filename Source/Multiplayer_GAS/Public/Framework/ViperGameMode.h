// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ViperGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	
	AViperGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* ExitingController) override;
	
	/**
	 * Server only. Takes every connected player to Map with seamless travel (clients keep their connection,
	 * PlayerController and PlayerState). Returns false if there is no map or a travel is already running.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Viper|Travel")
	bool StartGame(TSoftObjectPtr<UWorld> Map);
	
};
