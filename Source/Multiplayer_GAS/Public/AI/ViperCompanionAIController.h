// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ViperCompanionAIController.generated.h"

class UStateTreeAIComponent;
class USoundBase;

// Orders a player can give to their companion (bind them to inputs in the player character).
MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Order);
MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Order_Follow);
MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Order_MoveTo);

// StateTree event sent when the leader or the order changes.
MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Event_Reevaluate);

UENUM(BlueprintType)
enum class EViperCompanionStance : uint8
{
	/** Stay close to the leader. */
	Follow,
	/** Go to HoldLocation and stay there. */
	HoldPosition
};

/**
 * Brain of the companion (server only). It makes NO decisions: every decision lives in the StateTree.
 * It only stores memory (leader, stance, hold location, when it last talked, when it last took an idle break,
 * which side of the leader it walks on), offers actions the StateTree tasks use (Speak) and sends
 * Companion.Event.Reevaluate when the leader or the order changes.
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperCompanionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AViperCompanionAIController();

	/** Starts following NewLeader (nullptr = no leader, idle). Resets stance and memory. */
	void SetLeader(AController* NewLeader);

	/** Stores a Companion.Order.* order. Location / TargetActor come from where the player was aiming. */
	void IssueOrder(FGameplayTag OrderTag, const FVector& Location, AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	AController* GetLeader() const { return Leader.Get(); }

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	APawn* GetLeaderPawn() const;

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	EViperCompanionStance GetStance() const { return Stance; }

	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	FVector GetHoldLocation() const { return HoldLocation; }

	/***********************************************************************/
	/*                               VOICE                                 */
	/***********************************************************************/

	/** Seconds since the last voice line. */
	float GetTimeSinceLastVoice() const;

	/**
	 * Plays a voice line for the leader only. No cooldown here: the StateTree decides when to talk.
	 * Duration only feeds IsSpeaking(): the sound is a soft ref the server never loads, so it cannot
	 * measure the real length. Pass the length of the line (the Viper Speak task exposes it).
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	bool Speak(TSoftObjectPtr<USoundBase> VoiceLine, float Duration = 3.f);

	/** True while a voice line is assumed to still be playing. Drives the "look at me while talking" fact. */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	bool IsSpeaking() const;

	/***********************************************************************/
	/*                           IDLE / RHYTHM                             */
	/***********************************************************************/

	/** Called by the idle-break / wander tasks so the StateTree can space those out over time. */
	void NotifyIdleBreak();

	/** Seconds since the last idle break or wander. Huge the first time, so the tree may break early. */
	float GetTimeSinceLastIdleBreak() const;

	/***********************************************************************/
	/*                             FORMATION                               */
	/***********************************************************************/

	/** +1 = walks on the leader's right, -1 = on the left. Kept here so it survives state changes. */
	int32 GetFormationSide() const { return FormationSide; }

	/** Flips the side the companion walks on, so it does not trail in the leader's footsteps forever. */
	void FlipFormationSide();

	/***********************************************************************/
	/*                           INVESTIGATING                             */
	/***********************************************************************/

	/**
	 * The last thing she walked over to examine. Kept on the controller because the wander task is
	 * entered afresh for every leg, so its own instance data cannot remember across them, and
	 * without this she examines the nearest find over and over and never looks at anything else.
	 */
	AActor* GetLastExamined() const { return LastExamined.Get(); }
	void SetLastExamined(AActor* InActor) { LastExamined = InActor; }

	/***********************************************************************/
	/*                        STANDING SOMEWHERE                           */
	/***********************************************************************/

	/**
	 * How long the leader has been in one place, and where she has planted herself while they are.
	 * Here rather than in the follow task because the tree re-selects its states whenever any task
	 * finishes, which zeroes a task's instance data: measured, that rolled her a new bearing five
	 * times over a seventy-second wait, so she kept shifting about for no reason a player could
	 * see. Call UpdateStayingPut once a frame with the leader's position.
	 */
	void UpdateStayingPut(const FVector& LeaderLocation, float LoiterRadius, float DeltaTime);
	float GetLeaderStayingPutTime() const { return LeaderStayingPutTime; }

	/**
	 * What she is looking at while standing about. Chosen once per stop and kept here for the same
	 * reason as the spot: a task's instance data is zeroed on every state re-selection, and a gaze
	 * that is re-chosen every few seconds reads as a nervous tic.
	 */
	bool HasStandLook() const { return bHasStandLook; }
	FVector GetStandLook() const { return StandLook; }
	void SetStandLook(const FVector& InTarget) { StandLook = InTarget; bHasStandLook = true; }
	void ClearStandLook() { bHasStandLook = false; }

	bool HasPlantedSpot() const { return bHasPlantedSpot; }
	FVector GetPlantedSpot() const { return PlantedSpot; }
	void SetPlantedSpot(const FVector& InSpot) { PlantedSpot = InSpot; bHasPlantedSpot = true; }
	void ClearPlantedSpot() { bHasPlantedSpot = false; }

	UFUNCTION(BlueprintCallable, Category = "Viper|Companion")
	void SendCompanionEvent(FGameplayTag EventTag);

	/***********************************************************************/
	/*                             DEBUG TRACE                             */
	/***********************************************************************/

	/**
	 * The follow task reports what it is doing here so the evaluator can write the whole pair out on
	 * one timeline (the csv rows behind Viper.Companion.Csv). Nothing but the trace reads it.
	 * Mode: 0 standing, 1 steering straight at the slot, 2 following a path round something.
	 */
	void NotifyFollowDebug(float InIntent, int32 InMode, const FVector& InGoal = FVector::ZeroVector,
		const FVector& InSteer = FVector::ZeroVector)
	{
		DebugIntent = InIntent;
		DebugMode = InMode;
		DebugGoal = InGoal;
		DebugSteer = InSteer;
	}

	float GetDebugIntent() const { return DebugIntent; }
	int32 GetDebugMode() const { return DebugMode; }
	FVector GetDebugGoal() const { return DebugGoal; }
	FVector GetDebugSteer() const { return DebugSteer; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Viper|Companion")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

private:
	double GetWorldTime() const;

	TWeakObjectPtr<AController> Leader;
	EViperCompanionStance Stance = EViperCompanionStance::Follow;
	FVector HoldLocation = FVector::ZeroVector;

	double LastVoiceTime = -1.0e9;
	double SpeakingEndTime = -1.0e9;
	double LastIdleBreakTime = -1.0e9;

	int32 FormationSide = 1;

	TWeakObjectPtr<AActor> LastExamined;

	FVector LeaderStandAnchor = FVector::ZeroVector;
	bool bStandAnchorValid = false;
	float LeaderStayingPutTime = 0.f;
	FVector PlantedSpot = FVector::ZeroVector;
	bool bHasPlantedSpot = false;
	FVector StandLook = FVector::ZeroVector;
	bool bHasStandLook = false;

	float DebugIntent = 0.f;
	int32 DebugMode = 0;
	FVector DebugGoal = FVector::ZeroVector;
	FVector DebugSteer = FVector::ZeroVector;
};
