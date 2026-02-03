// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ViperActivatableWidget.h"
#include "ViperGameplayWidget.generated.h"

class UAbilitySystemComponent;
class UViperWidgetBar;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperGameplayWidget : public UViperActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	
private:
	UPROPERTY(meta=(BindWidget))
	UViperWidgetBar* HealthBar;

	UPROPERTY(meta=(BindWidget))
	UViperWidgetBar* ManaBar;

	UPROPERTY()
	UAbilitySystemComponent* OwnerAbilitySystemComponent;
};
