// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ViperOverallUILayout.h"
#include "GameFramework/HUD.h"
#include "ViperHUD.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	UViperOverallUILayout* GetOverallUILayout() const { return OverallUILayout; }
	
	
protected:
	
	UPROPERTY(BlueprintReadWrite)
	UViperOverallUILayout* OverallUILayout;
	
};
