// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactables/ViperObjectDefinition.h"
#include "ViperClueDefinition.generated.h"

class USoundBase;

/**
 * A piece of evidence. Name, description and icon come from UViperObjectDefinition (shown in the interaction prompt). Voice lines are said by the player's detective companion and only that player hears them.
 */
UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperClueDefinition : public UViperObjectDefinition
{
	GENERATED_BODY()

public:
	/** The detective can examine and note it by himself. If false only the player can (the detective just points it out). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue")
	bool bCanBeFoundByCompanion = true;

	/** Said when the clue is added to the notebook (found by the player or by the detective). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue|Voice")
	TSoftObjectPtr<USoundBase> NoteVoiceLine;

	/** Said once when the player walks near it without having found it ("hmm, look over there..."). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue|Voice")
	TSoftObjectPtr<USoundBase> NearbyVoiceLine;

	/** Said when the player is stuck and this is the closest clue left, or when the detective can't note it himself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue|Voice")
	TSoftObjectPtr<USoundBase> HintVoiceLine;
};
