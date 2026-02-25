// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "ViperCharacter.generated.h"

class UViperAbilitySystemComponent;
class UViperAttributeSet;
class IOnlineSession;

UCLASS()
class MULTIPLAYER_GAS_API AViperCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AViperCharacter();
	void ServerSideInit();
	void ClientSideInit();
	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/***********************************************************************/
	/*                          GAMEPLAY ABILITY                           */
	/***********************************************************************/

	UFUNCTION(BlueprintCallable, Category="Viper|Character")
	UViperAbilitySystemComponent* GetViperAbilitySystemComponent() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleDefaultsOnly, Category = "Gameplay Ability")
	UViperAbilitySystemComponent* ViperAbilitySystemComponent;

	UPROPERTY()
	UViperAttributeSet* ViperAttributeSet;

	//Pointer to the online session interface
	IOnlineSessionPtr OnlineSessionInterface;
	
	UFUNCTION(BlueprintCallable, Category = "Viper|Character")
	void CreateGameSession();
	
	UFUNCTION(BlueprintCallable, Category = "Viper|Character")
	void JoinGameSession();
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnFindSessionComplete(bool bWasSuccessful);
	
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
private:
	
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate; 
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
};
