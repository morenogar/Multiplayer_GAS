// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerController.h"
#include "NativeGameplayTags.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "ViperPlayerController.generated.h"

class UViperAbilitySystemComponent;
class UAbilitySystemComponent;
class UCommonActivatableWidget;
class UViperGameplayWidget;
class AViperCharacter;

MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UILayerID_Gameplay);
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperPlayerController : public APlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AViperPlayerController();

	//Only called on the server
	virtual void OnPossess(APawn* InPawn) override;

	//Only called on the client also on the listening server
	virtual void AcknowledgePossession(APawn* InPawn) override;

	UFUNCTION(BlueprintCallable)
	void SetGameplayWidget(UCommonActivatableWidget* Widget);

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintCallable, Category="Viper|PlayerController")
	UViperAbilitySystemComponent* GetViperAbilitySystemComponent() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
	{
		return GetViperAbilitySystemComponent();
	}

private:
	UPROPERTY()
	AViperCharacter* ViperPlayerCharacter;

	UPROPERTY()
	UViperGameplayWidget* GameplayWidget;

};
