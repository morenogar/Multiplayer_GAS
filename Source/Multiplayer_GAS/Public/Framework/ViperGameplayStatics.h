// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ViperGameplayStatics.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "worldContextObject"))
	static class AViperPlayerController* GetLocalPlayerController(const UObject* worldContextObject);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "worldContextObject"))
	static class AViperPlayerCharacter* GetLocalCharacter(const UObject* worldContextObject);
	
};
