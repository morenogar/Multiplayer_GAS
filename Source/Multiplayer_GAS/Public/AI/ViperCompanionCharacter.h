// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Interactables/ViperInteractableInterface.h"
#include "Interactables/ViperObjectDefinitionInterface.h"
#include "Player/ViperCharacter.h"
#include "ViperCompanionCharacter.generated.h"

class AViperCompanionAIController;
class AViperPlayerController;
class APlayerState;
class USphereComponent;
class UStateTree;
class UViperAbilitySet;

/**
 * What the companion is paying attention to. Set by the server (the Viper Look At / Idle Break tasks)
 * and replicated so every machine can aim the head: only the target travels over the network, never the
 * per-tick rotation, which each machine smooths locally in Tick.
 */
USTRUCT()
struct FViperLookAtState
{
	GENERATED_BODY()

	/** Actor being looked at. Takes priority over Location. May be null on a client until it becomes relevant. */
	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	/** World point to look at when Actor is null. */
	UPROPERTY()
	FVector_NetQuantize100 Location = FVector::ZeroVector;

	/** 0..1 how much the companion commits to the gaze. 0 = looking straight ahead. */
	UPROPERTY()
	float Weight = 0.f;

	/** True while a voice line is playing, so the mesh can play a talk animation. */
	UPROPERTY()
	bool bSpeaking = false;
};

/**
 * Detective companion placed in the level. A player recruits it by interacting with it (one companion per player);
 * interacting again with your own companion dismisses it.
 * Its brain is AViperCompanionAIController (server only) running CompanionStateTree.
 * Uses GAS like the players: AbilitySets are granted on the server when it's possessed.
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperCompanionCharacter : public AViperCharacter, public IViperInteractableInterface, public IViperObjectDefinitionInterface
{
	GENERATED_BODY()

public:
	AViperCompanionCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;

	/** IViperInteractableInterface. Runs on the interacting player's machine: asks the server to recruit / dismiss. */
	UFUNCTION()
	virtual void Interact(APlayerController* Player) override;

	/** IViperObjectDefinitionInterface: name shown in the interaction prompt. */
	UFUNCTION()
	virtual const UViperObjectDefinition* GetObjectDefinition() const override;

	/**
	 * Server. Recruits the companion for NewLeader (releasing NewLeader's previous companion).
	 * If it already follows NewLeader it's dismissed. Fails if it belongs to another player.
	 */
	bool HandleRecruitRequest(AViperPlayerController* NewLeader);

	/** Server. Stops following its leader and stays idle where it is. */
	void Release();

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	APlayerState* GetLeaderPlayerState() const { return LeaderPlayerState; }

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	bool IsRecruited() const { return LeaderPlayerState != nullptr; }

	UStateTree* GetCompanionStateTree() const { return CompanionStateTree; }
	float GetMaxRecruitDistance() const { return MaxRecruitDistance; }

	/***********************************************************************/
	/*                              LOOK AT                                */
	/***********************************************************************/

	/** Server. Look at an actor (its eyes, when it has any). Weight 0 releases the gaze. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion|LookAt")
	void SetLookAtActor(AActor* InActor, float InWeight = 1.f);

	/** Server. Look at a fixed world point (used for glancing around during idle breaks). */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion|LookAt")
	void SetLookAtLocation(const FVector& InLocation, float InWeight = 1.f);

	/** Server. Back to looking straight ahead. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion|LookAt")
	void ClearLookAt();

	/** Server. Marks the companion as talking for Duration seconds (replicated for the anim graph). */
	void SetSpeaking(bool bInSpeaking, float Duration = 0.f);

	/**
	 * Smoothed, neck-limit-clamped point the companion is looking at, in the mesh component's space.
	 * Feed this into the LookAtLocation pin of a "Look At" skeletal control node in the Anim Graph.
	 */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	FVector GetLookAtPointMeshSpace() const;

	/** Same point in world space (handy for debug draw or a Control Rig). */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	FVector GetLookAtPoint() const { return CurrentLookAtPoint; }

	/** 0..1 blend for the Look At node Alpha pin. Fades out when the target goes behind the companion. */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	float GetLookAtAlpha() const { return CurrentLookAtAlpha; }

	/** Gaze yaw relative to the actor forward, in degrees. For an Aim Offset, or to decide to turn the body. */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	float GetLookAtYaw() const { return CurrentLookAtYaw; }

	/** Gaze pitch relative to the actor forward, in degrees. */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	float GetLookAtPitch() const { return CurrentLookAtPitch; }

	UFUNCTION(BlueprintPure, Category = "Viper|Companion|LookAt")
	bool HasLookAtTarget() const;

	/** True while one of its voice lines is playing. Replicated: valid on clients too. */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	bool IsSpeaking() const { return LookAtState.bSpeaking; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Viper)
	FPrimaryAssetId ObjectDefinition;

protected:
	virtual void BeginPlay() override;

	/** Runs on every machine when the companion is recruited or dismissed (NewLeader is null when dismissed). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Viper|Companion", meta = (DisplayName = "On Leader Changed"))
	void BPE_OnLeaderChanged(APlayerState* NewLeader);

	/** StateTree run by the companion AI controller (schema: StateTree AI Component). */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion")
	TObjectPtr<UStateTree> CompanionStateTree;

	/** Abilities of the companion, e.g. animations used by the StateTree (Ability.Companion.Examine, Ability.Companion.TakeNotes). */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Abilities")
	TObjectPtr<UViperAbilitySet> AbilitySets;

	/** The player must be this close to recruit it (checked on the server). */
	UPROPERTY(EditAnywhere, Category = "Viper|Companion", meta = (ClampMin = "0"))
	float MaxRecruitDistance = 600.f;

	/** Lets the player interaction sphere (WorldDynamic overlaps) detect the companion. */
	UPROPERTY(VisibleAnywhere, Category = "Viper|Companion")
	TObjectPtr<USphereComponent> InteractionCollision;

	/***********************************************************************/
	/*                         LOOK AT TUNING                              */
	/***********************************************************************/

	/** Bone the gaze starts from. Falls back to the actor eye height when the bone does not exist. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt")
	FName LookAtSourceBone = TEXT("Head");

	/** A neck does not turn further than this. Past it the gaze is clamped instead of snapping around. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "0", ClampMax = "180"))
	float MaxLookAtYaw = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "0", ClampMax = "90"))
	float MaxLookAtPitch = 45.f;

	/** Degrees past MaxLookAtYaw over which the companion gives up and faces front again. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "1"))
	float LookAtYawFalloff = 30.f;

	/** How fast the gaze point chases the target. Low = lazy, heavy head. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "0.1"))
	float LookAtInterpSpeed = 7.f;

	/** How fast the gaze fades in and out. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "0.1"))
	float LookAtBlendSpeed = 3.5f;

	/** Distance of the neutral "looking straight ahead" point. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion|LookAt", meta = (ClampMin = "1"))
	float NeutralLookAtDistance = 600.f;

private:
	UFUNCTION()
	void OnRep_LeaderPlayerState();

	AViperCompanionAIController* GetCompanionController() const;

	/** Runs on every machine: resolves the replicated target and smooths the gaze. */
	void UpdateLookAt(float DeltaSeconds);

	/** World point the gaze starts from (head bone, or eye height as a fallback). */
	FVector GetLookAtSourceLocation() const;

	/** Resolves the replicated target into a world point. False when there is nothing to look at. */
	bool ResolveLookAtTarget(FVector& OutPoint) const;

	/** Server only: clears bSpeaking when the line is assumed to be over. */
	void OnSpeakingFinished();

	UPROPERTY(ReplicatedUsing = OnRep_LeaderPlayerState)
	TObjectPtr<APlayerState> LeaderPlayerState;

	UPROPERTY(Replicated)
	FViperLookAtState LookAtState;

	/** Server only. */
	TWeakObjectPtr<AViperPlayerController> LeaderController;

	/** Local, per-machine gaze state (never replicated). */
	FVector CurrentLookAtPoint = FVector::ZeroVector;
	float CurrentLookAtAlpha = 0.f;
	float CurrentLookAtYaw = 0.f;
	float CurrentLookAtPitch = 0.f;
	bool bLookAtInitialized = false;

	FTimerHandle SpeakingTimerHandle;
};
