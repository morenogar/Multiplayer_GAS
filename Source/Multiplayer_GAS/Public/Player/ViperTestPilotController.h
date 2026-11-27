// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/ViperPlayerController.h"
#include "ViperTestPilotController.generated.h"

/**
 * An AI that plays the player, so a test does not need anyone at the keyboard.
 *
 * It possesses an ordinary AViperPlayerCharacter - the same pawn a person would drive, so the
 * companion sees a leader exactly like the real thing - and takes its orders through navigation:
 * walk to that actor, find the nearest thing of that kind, look at this, wander about. That is the
 * difference from the scripted ViperQ steps already on the player controller, which replay a fixed
 * list: this one is told what to achieve and works the route out itself, so a map it has never
 * seen is a fair test rather than a recording.
 *
 * It is a PlayerController and not an AIController, which it was at first. The game's interaction
 * contract is IViperInteractableInterface::Interact(APlayerController*), and TriggerInteract casts
 * to one, so an AI-possessed pawn walked up to the companion and could never recruit her. Being a
 * It derives from AViperPlayerController, the project's own, not from APlayerController: the
 * companion recruits with Cast<AViperPlayerController>(Player) and silently does nothing for
 * anything else, so "INTERACT done" was true and she still had no leader. Being the project's
 * player controller means every "is this a player" test in the codebase answers yes, now and for
 * whatever gets written later, and nothing had to change to allow it. Movement comes from
 * SimpleMoveTo*, which gives a player controller the same path following an AI uses.
 *
 * Spawned on demand by the ViperPilot* console commands. Nothing creates one by itself and nothing
 * in the game depends on it: it exists for tests.
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperTestPilotController : public AViperPlayerController
{
	GENERATED_BODY()

public:
	AViperTestPilotController();

	/**
	 * Spawns a pawn of the game's own player class and possesses it.
	 *
	 * @param World      The world to spawn into.
	 * @param Where      Where to put it. Projected onto the navmesh first, so a rough spot is fine.
	 * @param PawnClass  The pawn to use. Null takes the game mode's default pawn, which is what a
	 *                   person would be driving.
	 * @return The pilot, or null when there is nowhere to stand or no pawn class to use.
	 */
	static AViperTestPilotController* Launch(UWorld* World, const FVector& Where,
		TSubclassOf<APawn> PawnClass = nullptr);

	/** Every pilot currently flying, so the console commands can talk to them. */
	static TArray<AViperTestPilotController*> All(UWorld* World);

	/** Walks to the actor and stops a short way off, as a person would. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderGoToActor(AActor* Target, float AcceptanceRadius = 120.f);

	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderGoToLocation(const FVector& Target, float AcceptanceRadius = 120.f);

	/**
	 * Finds the nearest actor whose name or class contains the filter and walks to it. This is the
	 * order the whole class is for: "go and find the thing" rather than "walk these corners".
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderFind(const FString& NameFilter, float AcceptanceRadius = 120.f);

	/**
	 * Walk to the nearest match and interact with it on arrival, which is the whole of recruiting
	 * the companion in one order. Two orders could not do it: a console fires them all at once,
	 * so the interact would happen before the walk had started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderFindAndInteract(const FString& NameFilter, float AcceptanceRadius = 150.f);

	/**
	 * A round of stops, walked one after another, each named by the same filter OrderFind takes.
	 *
	 * This is the only way to script a long test: the console fires every command in the same
	 * frame, so "go here, then there" cannot be two commands. Each stop is reached before the next
	 * is chosen, somewhere already visited is never chosen twice, and with bInteractAtEachStop it
	 * interacts on arrival - which is "recruit her, then take her round the level" in one line.
	 *
	 * @param CommaSeparatedStops e.g. "Companion,ViperItem,ViperItem,TargetPoint"
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderTour(const FString& CommaSeparatedStops, bool bInteractAtEachStop = false);

	/** A random reachable point within the radius, so the pilot can potter about on its own. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderWander(float Radius = 1200.f);

	/**
	 * Turns the pawn to face something without walking anywhere.
	 *
	 * @param HoldSeconds 0 turns once. Above 0 keeps re-aiming at the target every frame for that
	 *                    long, so a target that moves (the companion walking over) stays looked at.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderLookAt(AActor* Target, float HoldSeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	void OrderStop();

	/** Interacts with whatever is in front of the pawn, the way the interact input would. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	bool OrderInteract();

	/** One line about what it is doing, for the console and the logs. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	FString Describe() const;

	/** Unpossesses and destroys both the pilot and its pawn. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	void Land();

	/** Nearest actor to the pawn whose name or class contains the filter, ignoring case. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Test")
	AActor* FindNearest(const FString& NameFilter) const;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	/**
	 * The pawn's eyes, looking where the pilot is looking.
	 *
	 * Everything that asks "what is this character looking at" goes through here, the interact
	 * trace among them. A player controller would normally answer with its camera manager, which
	 * a pilot nobody is watching has no reason to keep pointed anywhere in particular.
	 */
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

private:
	/** True while a SimpleMoveTo is still running, so arrival can be noticed once. */
	bool bMoving = false;

	/** What it was last told to do, so Describe and the arrival log can say something useful. */
	UPROPERTY()
	FString LastOrder;

	UPROPERTY()
	TWeakObjectPtr<AActor> OrderTarget;

	/** Held gaze from OrderLookAt: re-aimed at every frame until LookUntil (world seconds). */
	TWeakObjectPtr<AActor> LookTarget;
	double LookUntil = 0.0;

	/** Points the control rotation and the pawn at the target. */
	void AimAt(const AActor& Target);

	/** Set by OrderFindAndInteract, honoured once the walk finishes. */
	UPROPERTY()
	bool bInteractOnArrival = false;

	/** Moves to the next stop of a tour, or ends it. Called a moment after each arrival. */
	void AdvanceTour();

	/** Nearest match that is not one of these, so a tour of three crates visits three crates. */
	AActor* FindNearestIgnoring(const FString& NameFilter, const TArray<TWeakObjectPtr<AActor>>& Ignore) const;

	UPROPERTY()
	TArray<FString> TourStops;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> TourVisited;

	int32 TourIndex = INDEX_NONE;
	bool bTourInteracts = false;

	double OrderStartedAt = 0.0;
};
