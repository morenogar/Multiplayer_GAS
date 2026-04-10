// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViperAbilitySystemInterface.generated.h"

class UAbilitySystemComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UViperAbilitySystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYER_GAS_API IViperAbilitySystemInterface
{
	GENERATED_BODY()

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const = 0;
};
