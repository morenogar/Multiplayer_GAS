// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactables/ViperInteractableInterface.h"
#include "Interactables/ViperObjectDefinitionInterface.h"
#include "ViperClueActor.generated.h"

class APlayerController;
class USphereComponent;
class UStaticMeshComponent;
class UViperClueDefinition;

/**
 * Evidence placed in the level. The player examines it by interacting with it: the server validates it and fires
 * On Examined (Blueprint) so you decide what happens. No AI logic here.
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperClueActor : public AActor, public IViperObjectDefinitionInterface, public IViperInteractableInterface
{
	GENERATED_BODY()

public:
	AViperClueActor();

	/** IViperObjectDefinitionInterface: the clue definition (name for the interaction prompt). */
	UFUNCTION()
	virtual const UViperObjectDefinition* GetObjectDefinition() const override;

	/** IViperInteractableInterface. Runs on the interacting player's machine: asks the server to examine it. */
	UFUNCTION()
	virtual void Interact(APlayerController* Player) override;

	UFUNCTION(BlueprintPure, Category = "Viper|Clue")
	UViperClueDefinition* GetClueDefinition() const { return ClueDefinition; }

	float GetMaxExamineDistance() const { return MaxExamineDistance; }

	/** Server. Called by AViperPlayerController::ServerExamineClue after validating the distance. */
	void NotifyExamined(APlayerController* ExaminingPlayer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Runs on the server when a player examines this clue. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Viper|Clue", meta = (DisplayName = "On Examined"))
	void BPE_OnExamined(APlayerController* ExaminingPlayer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Viper|Clue")
	TObjectPtr<UViperClueDefinition> ClueDefinition;

	/** The player must be this close to examine it (checked on the server). */
	UPROPERTY(EditAnywhere, Category = "Viper|Clue", meta = (ClampMin = "0"))
	float MaxExamineDistance = 500.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Viper|Clue")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Lets the player's interaction sphere (WorldDynamic overlaps) detect the clue. */
	UPROPERTY(VisibleAnywhere, Category = "Viper|Clue")
	TObjectPtr<USphereComponent> InteractionCollision;
};
