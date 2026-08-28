// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ViperCharacter.generated.h"

class UViperAbilitySystemComponent;
class UViperAttributeSet;

UCLASS()
class MULTIPLAYER_GAS_API AViperCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AViperCharacter();
	void ServerSideInit();
	void ClientSideInit();
	
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

};
