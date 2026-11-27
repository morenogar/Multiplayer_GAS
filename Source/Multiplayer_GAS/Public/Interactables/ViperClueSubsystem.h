// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ViperClueSubsystem.generated.h"

class AViperClueActor;

/** Registry of the clues in the current world (they register themselves in BeginPlay). */
UCLASS()
class MULTIPLAYER_GAS_API UViperClueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterClue(AViperClueActor* Clue);
	void UnregisterClue(AViperClueActor* Clue);

	const TArray<TObjectPtr<AViperClueActor>>& GetClues() const { return Clues; }

	/** Closest clue to Location that passes Filter. MaxDistance <= 0 means any distance. */
	AViperClueActor* FindClosestClue(const FVector& Location, float MaxDistance, TFunctionRef<bool(const AViperClueActor&)> Filter) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<AViperClueActor>> Clues;
};
