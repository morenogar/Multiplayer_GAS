// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ViperInteractableInterface.h"
#include "ViperObjectDefinitionInterface.h"
#include "GameFramework/Actor.h"
#include "ViperAbilityActor.generated.h"

class UViperInteractResponse;

UCLASS()
class MULTIPLAYER_GAS_API AViperAbilityActor : public AActor, public IViperObjectDefinitionInterface, public IViperInteractableInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AViperAbilityActor();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Viper)
	FPrimaryAssetId ObjectDefinition;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Viper, meta=(AllowedTypes="ViperObject"))
	TArray<UViperInteractResponse*> InteractResponses;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	
	UFUNCTION()
	virtual const UViperObjectDefinition* GetObjectDefinition() const override;
	
	UFUNCTION()
	virtual void Interact(APlayerController* Player);
	
};
