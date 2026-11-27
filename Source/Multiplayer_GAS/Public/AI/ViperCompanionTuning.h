// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ViperCompanionTuning.generated.h"

/**
 * Live overrides for the companion movement parameters.
 *
 * The parameters themselves belong to the Viper Follow Leader task and live inside ST_Companion,
 * which is an asset: changing one means opening the StateTree editor, editing, compiling and
 * restarting. That is far too slow a loop for deciding how a walk should feel, and impossible in a
 * packaged build. So the task consults this object every tick and lets it win while bEnabled is on.
 *
 * One TMap rather than a property per parameter, on purpose. The keys are the real C++ property
 * names on FViperFollowLeaderTaskInstanceData, looked up by reflection, so adding a parameter to
 * the task needs no change here at all, and a name that does not exist is reported instead of
 * silently doing nothing. Names the task does not have are tried against the character movement
 * component next, which is how MaxAcceleration and the rest are reachable too.
 *
 * Three ways to write it, all reaching the same function:
 *   - the console: ViperTune FormationDistance 260
 *   - Project Settings -> Viper -> Companion Tuning, with the game running in the editor
 *   - the Remote Control API over HTTP, which is what the browser tuner uses:
 *     PUT /remote/object/call  { objectPath: "/Script/Multiplayer_GAS.Default__ViperCompanionTuning",
 *                                functionName: "SetOverride", parameters: { Name: "...", Value: 0 } }
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Companion Tuning"))
class MULTIPLAYER_GAS_API UViperCompanionTuning : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Settings object, always valid. */
	static UViperCompanionTuning* Get();

	virtual FName GetCategoryName() const override;

	/** Nothing below is applied while this is off, so the asset stays in charge until you ask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Companion")
	bool bEnabled = false;

	/**
	 * Parameter name to value. Booleans take 0 or 1. A key is matched against the follow task
	 * first and the character movement component second; RotationRate means its yaw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Companion")
	TMap<FName, float> Overrides;

	/** Sets one override and switches overriding on. Returns false if no such parameter exists. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	bool SetOverride(FName Name, float Value);

	/** Drops one override, so that parameter goes back to what the asset says. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	bool ClearOverride(FName Name);

	/** Drops every override and switches overriding off. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	void ClearAll();

	/** Writes the overrides into Config/DefaultGame.ini so they survive a restart. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	void SaveToConfig();

	/** True when Name is a parameter that can be overridden, which is what makes a typo reportable. */
	static bool IsKnownParameter(FName Name);

	/** Every name that can be overridden, so the console can show them instead of being guessed at. */
	static TArray<FName> ListParameters();
};
