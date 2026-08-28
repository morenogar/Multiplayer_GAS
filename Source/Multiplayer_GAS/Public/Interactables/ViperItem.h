// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ViperItem.generated.h"

UCLASS()
class MULTIPLAYER_GAS_API AViperItem : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AViperItem();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	

};
