// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ViperInteractResponse.generated.h"

class UCommonActivatableWidget;
class AViperPlayerController;

UENUM(BlueprintType)
enum class ETargetingInteractResponseType : uint8
{
	// Use for specific responses? Do we need a local response?
	None,
	OpenMenu
};


/**
 * 
 */
UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperInteractResponse : public UObject
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETargetingInteractResponseType Type;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ETargetingInteractResponseType::OpenMenu", EditConditionHides))
	TSubclassOf<UUserWidget> MenuToOpen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "UI.Layer", EditCondition = "Type == ETargetingInteractResponseType::OpenMenu", EditConditionHides))
	FGameplayTag MenuUILayer;
	
	void Interact(APlayerController* Player);
	
	UFUNCTION()
	void MenuClosed();
	
protected:
	
	UPROPERTY()
	UCommonActivatableWidget* InteractResponseWidget;

};
