// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvaluatorBase.h"
#include "AI/ViperCompanionAIController.h"
#include "Tasks/StateTreeAITask.h"
#include "ViperCompanionStateTreeNodes.generated.h"

class AAIController;
class USoundBase;

/***********************************************************************/
/*                     EVALUATOR: VIPER COMPANION                      */
/***********************************************************************/

/**
 * Everything the companion decides on, written out so a play session can be read back as text.
 * Driven by the console variable Viper.Companion.Debug: 0 off, 1 a summary a few times a second,
 * 2 the same plus every mode change the moment it happens.
 */
MULTIPLAYER_GAS_API DECLARE_LOG_CATEGORY_EXTERN(LogViperCompanion, Log, All);

USTRUCT()
struct FViperCompanionEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/*------------------------------- Parameters -------------------------------*/

	/** Below this speed the leader counts as standing still. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float LeaderMovingSpeedThreshold = 20.f;

	/**
	 * The widest the aim test ever opens up, in degrees off the leader view direction. It is a ceiling,
	 * not the test itself: the real threshold is how much of the view the companion body actually fills
	 * at that distance, so pointing at them means the same thing up close and across a clearing. A fixed
	 * cone does not: 30 degrees is a four-metre-wide fan at eight metres, which catches a leader who is
	 * plainly looking past them.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "0", ClampMax = "180"))
	float EyeContactAngle = 18.f;

	/** The narrowest it ever closes to, so aiming at someone far away stays humanly possible. */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "0", ClampMax = "90"))
	float MinEyeContactAngle = 3.5f;

	/** Half-width of the body the aim test has to land on, in world units. */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "1"))
	float EyeContactBodyRadius = 55.f;

	/** Multiplies that half-width. 1 is dead on the body; 2 gives a forgiving margin without being a cone. */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "0.1", ClampMax = "6"))
	float EyeContactTolerance = 2.f;

	/** The leader has to be this close for eye contact to register. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float EyeContactDistance = 900.f;

	/**
	 * How long the leader has to hold their aim on the companion before it counts as being looked
	 * at. Reacting on the first frame is what makes a companion feel like a tripwire; a beat of
	 * delay reads as someone realising you are waiting for them.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "0"))
	float EyeContactHoldTime = 0.8f;

	/**
	 * How long a glance away is forgiven before the clock resets. Without it an aim wobbling
	 * across the edge of the body flicks the companion in and out of attention several times a
	 * second, which is worse than either state.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (ClampMin = "0"))
	float EyeContactBreakGrace = 0.5f;

	/** Shared attention: how long you have to keep looking at the same thing before she looks at it too. */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "0"))
	float FocusHoldTime = 1.5f;

	/** How far the point you look at may drift and still count as the same thing. */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "10"))
	float FocusRadius = 150.f;

	/** Further than this, what you look at is scenery, not something to share. */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "100"))
	float FocusMaxDistance = 1500.f;

	/**
	 * An interactable within this many degrees of your view is what you are looking at, even when
	 * the crosshair is on the table under it.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "0", ClampMax = "45"))
	float FocusSnapAngle = 8.f;

	/**
	 * Rough size of a thing to examine. Up close it fills more of the view than FocusSnapAngle, so
	 * the margin grows to match - the same idea as the eye-contact test.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "1"))
	float FocusTargetRadius = 60.f;

	/** A glance away shorter than this does not reset the clock. */
	UPROPERTY(EditAnywhere, Category = "Parameter|SharedAttention", meta = (ClampMin = "0"))
	float FocusBreakGrace = 0.4f;

	/**
	 * How long the leader has to stay put, by the test below, before they count as having settled
	 * somewhere and the companion is free to go about her own business.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Roaming", meta = (ClampMin = "0"))
	float LeaderSettleTime = 8.f;

	/**
	 * How far the leader may get from the anchor and still count as staying put. Standing dead
	 * still was too strict a test: it read as forced, since shifting your feet put the companion
	 * straight back at heel, and it missed the case that matters most - someone poking about one
	 * room is staying put however much they are walking. Keep it well under a few seconds of
	 * walking, so that actually going somewhere crosses it at once and never reads as loitering.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Roaming", meta = (ClampMin = "0"))
	float LeaderLoiterRadius = 400.f;

	/**
	 * How far the leader may move from the anchor while still counting as settled there. Make it
	 * the same as the wander radius and it is one idea rather than two: the area the companion is
	 * free to move about in, which the leader may also move about in without ending her business.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Roaming", meta = (ClampMin = "0"))
	float LeaderSettleRadius = 750.f;

	/**
	 * Seconds the leader has to keep walking away from the anchor, already outside
	 * LeaderLoiterRadius, before they count as leaving the area. Without it she kept to her own
	 * business until they were the whole LeaderSettleRadius away - two seconds or more of walking -
	 * and only then started her reaction. Short: it only has to tell heading off from a step
	 * outwards while pacing about. 0 = leave only at LeaderSettleRadius, the old behaviour.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Roaming", meta = (ClampMin = "0"))
	float LeaveAreaConfirmTime = 0.3f;

	/** How often RandomValue is re-rolled. A value that holds for a while, so states do not flicker. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1"))
	float RandomRerollInterval = 2.f;

	/*-------------------------------- Outputs ---------------------------------*/

	/** Pawn of the player the companion follows (null when not recruited or while the leader respawns). */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Leader = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasLeader = false;

	UPROPERTY(EditAnywhere, Category = Output)
	float DistanceToLeader = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	EViperCompanionStance Stance = EViperCompanionStance::Follow;

	/** Where to wait when Stance is HoldPosition (Companion.Order.MoveTo). */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector HoldLocation = FVector::ZeroVector;

	/** How far the companion still is from HoldLocation. 0 when there is no hold order. */
	UPROPERTY(EditAnywhere, Category = Output)
	float DistanceToHoldLocation = 0.f;

	/** Seconds since the companion last said something. */
	UPROPERTY(EditAnywhere, Category = Output)
	float TimeSinceLastVoice = 0.f;

	/** True while one of its voice lines is still playing: the cue to hold eye contact with the leader. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bIsSpeaking = false;

	/** Ground speed of the leader. Lets the companion walk when you walk and run when you run. */
	UPROPERTY(EditAnywhere, Category = Output)
	float LeaderSpeed = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bLeaderIsMoving = false;

	/** True when the leader is close and aiming their view at the companion: the other half of eye contact. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bLeaderLooksAtMe = false;

	/** Degrees between where the leader is looking and the companion. Read it in the log to tune the aim test. */
	UPROPERTY(EditAnywhere, Category = Output)
	float LeaderAimErrorDegrees = 0.f;

	/** What LeaderAimErrorDegrees is being measured against right now: it tightens with distance. */
	UPROPERTY(EditAnywhere, Category = Output)
	float EyeContactThresholdDegrees = 0.f;

	/**
	 * Seconds the leader has been holding their aim on the companion, forgiving short glances away.
	 * This is what lets the tree tell a look from a glance, and a long look from a short one: the
	 * same fact gates stopping to give attention and, past a longer threshold, walking over.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	float TimeLookedAt = 0.f;

	/** TimeLookedAt has passed EyeContactHoldTime: the look has been held long enough to mean something. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bLeaderNoticesMe = false;

	/** What you are looking at: an interactable in your sights, else where your view ray lands. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector LeaderFocusPoint = FVector::ZeroVector;

	/** The interactable you are looking at, or null for a plain point. Bind Look At's Target to it. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> LeaderFocusActor = nullptr;

	/** Seconds you have kept looking at the same thing (short glances away forgiven). */
	UPROPERTY(EditAnywhere, Category = Output)
	float TimeLeaderFocused = 0.f;

	/** You have looked at something that is not her for FocusHoldTime: worth sharing. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bLeaderStudying = false;

	/**
	 * Seconds the leader has been staying put - within LeaderLoiterRadius of the anchor, whether
	 * walking about in there or standing still. The cue for the companion to stop waiting on them.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	float TimeLeaderStayingPut = 0.f;

	/**
	 * Where the leader was when the staying-put clock started, and how far they are from it. The
	 * companion roams in an area centred here rather than on the leader, so a couple of steps do
	 * not drag her territory along; and the tree can end the roaming the moment the leader leaves
	 * it, by comparing the distance below against the same radius the wander task is using.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector LeaderAnchor = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	float DistanceLeaderFromAnchor = 0.f;

	/**
	 * The leader has settled at the anchor: still long enough to count, and not yet far enough from
	 * it to count as on the move again. Latched on purpose, and the fact to gate roaming on rather
	 * than TimeLeaderStayingPut. The wander task finishes each time it arrives, so it is selected
	 * again, and a raw idle time is zero by then if the leader has taken a single step - which ends
	 * the companion's own business every time the leader shifts their feet.
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bLeaderSettled = false;

	/** False when a wall is in the way. Use it to gate teleporting and to decide to catch up. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasLineOfSightToLeader = false;

	/** Seconds since the last idle break or wander. Huge at first, so the tree may break early. */
	UPROPERTY(EditAnywhere, Category = Output)
	float TimeSinceLastIdleBreak = 0.f;

	/** 0..1, re-rolled every RandomRerollInterval seconds. Compare against it so behaviour is not identical every time. */
	UPROPERTY(EditAnywhere, Category = Output)
	float RandomValue = 0.f;

	/*------------------------------- Internal ---------------------------------*/

	UPROPERTY()
	float RandomRerollCountdown = 0.f;

	UPROPERTY()
	float TraceCountdown = 0.f;

	UPROPERTY()
	float CsvCountdown = 0.f;

	/** Seconds since the aim test last passed, for the grace period above. */
	UPROPERTY()
	float AimLostTime = 0.f;

	/** The anchor is only meaningful once the leader has stood still at least one tick. */
	UPROPERTY()
	bool bAnchorValid = false;

	/** Seconds the leader has been walking out of the area, for LeaveAreaConfirmTime. */
	UPROPERTY()
	float LeavingAreaTime = 0.f;

	UPROPERTY()
	bool bWroteCsvHeader = false;

	/** Seconds the current focus has not been looked at, for FocusBreakGrace. */
	UPROPERTY()
	float FocusLostTime = 0.f;

	/** LeaderFocusPoint holds something real (not the sky, not her). */
	UPROPERTY()
	bool bFocusValid = false;
};

/** Turns the controller memory into facts for the StateTree (no decisions here). Add it once as a global evaluator. */
USTRUCT(meta = (DisplayName = "Viper Companion", Category = "Viper|Companion"))
struct FViperCompanionEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperCompanionEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/***********************************************************************/
/*                      TASK: VIPER FOLLOW LEADER                      */
/***********************************************************************/

USTRUCT()
struct FViperFollowLeaderTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Bind it to the Leader output of the Viper Companion evaluator. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> Leader = nullptr;

	/*--------------------------- Formation slot -------------------------------*/

	/** How far behind the leader the companion aims to walk. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float FormationDistance = 220.f;

	/** Degrees off the leader back, so it walks beside them instead of in their footsteps. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0", ClampMax = "90"))
	float FormationAngle = 35.f;

	/** Seconds between side swaps (left shoulder / right shoulder). 0 = never swap. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float SideSwapInterval = 9.f;

	/**
	 * Seconds the swap itself takes. The slot sits two metres off one shoulder, so flipping the side
	 * in one frame moves it four metres across the leader back and the companion sets off sideways to
	 * chase it. Swept over a couple of seconds the same swap reads as drifting across to your other
	 * side while you both walk, which is what people actually do.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float SideSwapTime = 2.f;

	/**
	 * While the leader stands still, stop deriving the slot from their facing and only correct the
	 * distance along the bearing the companion already stands on. The slot sits behind the leader, so
	 * turning on the spot drags it round in a circle and the companion walks that circle: the single
	 * thing most often described as "it orbits me". Nobody repositions because you looked left.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation")
	bool bHoldBearingWhileLeaderStands = true;

	/** Seconds of leader velocity added to the goal, so it cuts corners instead of trailing the path. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0", ClampMax = "2"))
	float LeadTime = 0.35f;

	/**
	 * Closer than this to the leader and it steers away while it walks. Reaching the slot is not worth
	 * brushing past someone: people hold a gap even when it costs them a step, and a companion that
	 * does not is the one thing everybody reads as "it is not really there with me".
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float PersonalSpaceRadius = 150.f;

	/*---------------------------- Start / stop --------------------------------*/

	/** Starts walking once the slot is farther than this. Keep it well above StopDistance. */
	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float StartFollowDistance = 320.f;

	/** Stops once it gets this close. The gap with StartFollowDistance is what kills the stutter. */
	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float StopFollowDistance = 170.f;

	/** Random slack added to the stop distance each time, so it never parks in the same spot twice. */
	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float StopDistanceJitter = 70.f;

	/**
	 * Above this leader speed the companion never stops, whatever the distance says. Stopping is a reply
	 * to the leader stopping, not to the slot being briefly within reach: without this it halts, lets the
	 * slot run off, starts again, and the walk turns into a series of lunges that also loses ground.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float KeepWalkingLeaderSpeed = 20.f;

	/**
	 * Start and stop distances used instead of the two above while the leader stands still, where the
	 * judgement is a different one: no slot is running away, there is only a gap left over from the
	 * walk. The wide band that kills the stutter while walking is wider than that leftover, so the
	 * companion parks two and a half metres further out than the formation distance says and stays
	 * there - measured at a 4.7 m gap for a 2.2 m formation. People close that up when you stop.
	 * 0 turns this off and uses the walking band throughout.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float SettleStartDistance = 130.f;

	UPROPERTY(EditAnywhere, Category = "Parameter|StartStop", meta = (ClampMin = "0"))
	float SettleStopDistance = 55.f;

	/*----------------------------- Speed match --------------------------------*/

	/** Speed used when the leader is strolling or the companion is nearly there. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float WalkSpeed = 200.f;

	/** Speed used when it has ground to make up. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float RunSpeed = 520.f;

	/** Distance at which it commits to RunSpeed. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "1"))
	float SpeedMatchDistance = 750.f;

	/** How much the leader own speed pulls the companion speed up. 0 = distance only. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0", ClampMax = "1"))
	float LeaderSpeedInfluence = 0.6f;

	/**
	 * While it is within this much of the slot and the leader is walking, the companion simply walks
	 * at the leader pace instead of solving for the distance. Two people walking together hold one
	 * pace and let the gap breathe; a speed that answers the gap every frame is a servo tracking a
	 * setpoint, and it reads as one even when the numbers stay small.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float PaceMatchTolerance = 260.f;

	/**
	 * How fast the commanded speed may change, in units per second squared. Legs do not change pace
	 * instantly, and the slot swings enough that the raw answer jumps between walk and run several
	 * times in what is meant to be a straight line together. 0 = no limit.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float SpeedChangeRate = 450.f;

	/*------------------------------ Pathing -----------------------------------*/

	/** Minimum seconds between repaths while chasing a moving slot. Keeps the path from thrashing. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing", meta = (ClampMin = "0.05"))
	float RepathInterval = 0.4f;

	/** Only repaths early when the slot has drifted at least this far. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing", meta = (ClampMin = "0"))
	float RepathTolerance = 120.f;

	/** Teleports next to the leader when farther than this (0 = never teleport). */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing", meta = (ClampMin = "0"))
	float TeleportDistance = 3000.f;

	/** Only teleport while the leader cannot see the companion, so it never pops in front of you. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing")
	bool bOnlyTeleportWhenUnseen = true;

	/**
	 * Steers straight at the slot while the way is clear, and only asks for a path when something is
	 * in between. Every path request restarts the follow, so asking a few times a second is what makes
	 * a companion read as a machine: it re-aims at the first corner of each new path.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing")
	bool bPreferDirectSteering = true;

	/** Seconds between the navmesh checks that pick between steering and pathing. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing", meta = (ClampMin = "0.05"))
	float ObstacleCheckInterval = 0.25f;

	/**
	 * How far it has to be able to walk down the straight line before it steers rather than asks for
	 * a path. The line to a slot two metres off your shoulder leaves the walkable strip constantly
	 * without anything solid being in the way, and asking for a path every time is what made the
	 * pace surge: the steps spent pathing showed 62 pace changes per ten seconds against 2 for the
	 * steps spent steering. Below this it is genuinely boxed in and the navmesh earns its keep.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Pathing", meta = (ClampMin = "0"))
	float MinSteerReach = 150.f;

	/*------------------------------- Easing -----------------------------------*/

	/** Seconds to work up to walking with intent, so it leans into the move instead of snapping to speed. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0"))
	float IntentRiseTime = 0.45f;

	/**
	 * Seconds spent easing off before it stands still: the window where it keeps walking while it is
	 * already giving you its attention. Nobody stops dead when a walking companion looks at them.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0"))
	float IntentFallTime = 1.2f;

	/** Seconds of smoothing on the leader heading. The slot swings with it, so a raw heading makes the goal shiver. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0"))
	float HeadingSmoothTime = 0.5f;

	/**
	 * Easing-off time used instead of IntentFallTime while settling up to a leader who has stopped.
	 * The long one exists for the moment you look at a companion who is still walking, and it costs
	 * about one metre twenty of glide. Spend that walking up to someone standing still and it sails
	 * straight past the spot it was aiming for and parks a metre and a half from them: measured at
	 * twenty seconds of a run spent that close. Walking up to someone and stopping is a short,
	 * deliberate halt, not a long drift. 0 falls back to IntentFallTime.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0"))
	float SettleFallTime = 0.4f;

	/**
	 * Seconds of smoothing on the steering direction itself. Everything that can move the goal inside
	 * one frame - a side swap, the personal-space push coming on, the slot crossing behind the leader
	 * - arrives here as a direction that flips, and a body that changes heading in one frame is the
	 * clearest tell there is. Last filter before the pawn, so it catches all of them at once.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0"))
	float SteerSmoothTime = 0.25f;

	/**
	 * Once it is near its slot and the leader is walking, how much of its heading comes from the
	 * leader direction of travel instead of from the slot. Aiming a body at a point two metres off
	 * someone shoulder means the aim swings hard as that point drifts, and a fifth of a measured
	 * walk was spent turning faster than 200 degrees a second because of it. People walking
	 * together match direction and let a small sideways error wash out over the next few strides.
	 * 0 aims at the slot throughout.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadingFollowWeight = 0.75f;

	/** Enter already easing off: for a state where it comes to rest to give you its attention. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing")
	bool bComeToRest = false;

	/**
	 * How far away it settles while coming to rest, measured along the bearing it already stands on.
	 * It deliberately ignores the formation slot: walking round to the leader shoulder in order to stop
	 * and talk is what reads as orbiting them. People stop where they are and turn.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Easing", meta = (EditCondition = "bComeToRest", ClampMin = "0"))
	float ConversationDistance = 230.f;

	/*----------------------------- Attention ----------------------------------*/

	/**
	 * Once it has come to a stop near the leader, turn to face them. Standing there still pointing
	 * the way it last walked is what makes a companion look switched off between orders. It gives way
	 * the moment anything else takes the wheel (the look-at task turns the body through the
	 * controller), so the two never fight over the same yaw.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention")
	bool bFaceLeaderWhenStopped = true;

	/** Degrees per second for that turn. Slower than the walking turn rate: it is a glance, not a snap. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention", meta = (EditCondition = "bFaceLeaderWhenStopped", ClampMin = "1"))
	float FaceLeaderTurnRate = 160.f;

	/**
	 * Standing still, she looks at the nearest thing worth examining, or else down a way she could
	 * still walk. Off, she faces the leader every time she halts, which is what she used to do and
	 * reads as a pet waiting for orders - and parked against a wall she faced him through it. She
	 * looks back regardless whenever the leader is looking at her, which is bComeToRest.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention")
	bool bLookAroundWhenStopped = true;

	/** How far off something examinable may be and still be what she looks at while standing. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention", meta = (ClampMin = "0"))
	float StandLookRange = 900.f;

	/**
	 * How strongly the way she walked in beats the way she came from when choosing where to look.
	 * With open ground both ahead and behind, the plain furthest-point rule had no opinion and
	 * would sometimes turn her round on arrival, which reads as a glitch. 0 is that old behaviour;
	 * 1 all but forbids looking back. It ranks rather than vetoes, so in a dead end - where the
	 * only way out is behind her - she still turns round.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention", meta = (ClampMin = "0", ClampMax = "1"))
	float LookForwardBias = 0.75f;

	/*--------------------------- Standing somewhere ---------------------------*/

	/**
	 * How far the leader may get from where they stopped and still count as being in one place.
	 * The same question the evaluator's LeaderLoiterRadius asks, answered again here so the task
	 * needs no binding to stand on its own.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Settling", meta = (ClampMin = "0"))
	float SettleLoiterRadius = 400.f;

	/**
	 * How long the leader has to have been in one place before she chooses somewhere to stand
	 * rather than merely stopping. Without the delay, every two-second pause on a walk has her
	 * pick a new place and walk to it, which reads as fidgeting.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Settling", meta = (ClampMin = "0"))
	float SettlePlantDelay = 2.5f;

	/**
	 * How far off dead-behind the chosen spot may be, rolled afresh each time. 0 puts her forever
	 * directly behind the leader, which is where the plain slot always left her.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Settling", meta = (ClampMin = "0", ClampMax = "170"))
	float SettleBearingSpread = 105.f;

	/** Only bother turning while the leader is this close (0 = any distance). */
	UPROPERTY(EditAnywhere, Category = "Parameter|Attention", meta = (EditCondition = "bFaceLeaderWhenStopped", ClampMin = "0"))
	float FaceLeaderDistance = 700.f;

	/*---------------------------- Reaction time -------------------------------*/

	/**
	 * Seconds between deciding to set off and actually setting off, drawn from a normal distribution
	 * with this mean. A person who sees you walk away takes a beat to follow; starting on the very
	 * frame you do is the tell of a machine. Applies to starting only: stopping and carrying on
	 * while already walking are never delayed. 0 switches it off.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Reaction", meta = (ClampMin = "0", ClampMax = "2"))
	float ReactionTimeMean = 0.35f;

	/** Standard deviation of the reaction time. Samples are clamped to [0.43, 2] x the mean (0.15-0.7 s by default). */
	UPROPERTY(EditAnywhere, Category = "Parameter|Reaction", meta = (ClampMin = "0", ClampMax = "1"))
	float ReactionTimeSpread = 0.1f;

	/*----------------------------- Out of the way -----------------------------*/

	/**
	 * Half-angle of the cone ahead of the leader that counts as "where they are about to walk". A
	 * standing spot inside it is rejected, and a companion inside it while they walk at her steps
	 * aside. People do not stand where you are obviously heading.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|OutOfTheWay", meta = (ClampMin = "0", ClampMax = "90"))
	float LeaderPathConeAngle = 35.f;

	/** Length of that cone when choosing where to stand. */
	UPROPERTY(EditAnywhere, Category = "Parameter|OutOfTheWay", meta = (ClampMin = "0"))
	float LeaderPathConeLength = 400.f;

	/**
	 * A standing spot needs at least this much free width across it (navmesh width plus her own
	 * body). Narrower is a doorway or a gap between props, and standing in one blocks it.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|OutOfTheWay", meta = (ClampMin = "0"))
	float MinStandWidth = 140.f;

	/**
	 * When the leader walks at her and she is in their path cone within this distance, she steps
	 * aside to the side she is already on instead of backing up in front of them. 0 = off.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|OutOfTheWay", meta = (ClampMin = "0"))
	float StepAsideDistance = 250.f;

	/*---------------------- Side by side / single file ------------------------*/

	/**
	 * The formation adapts to the space. Strolling slowly in the open, people walk beside each other;
	 * in a corridor they fall in behind. The free width across the leader's heading is measured a few
	 * times a second and the slot blends towards one of the two shapes over FormationBlendTime.
	 * Below this leader speed (and with open ground) she walks beside them.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float SideBySideMaxLeaderSpeed = 220.f;

	/** Free width across the leader's heading above which the ground counts as open. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float SideBySideMinWidth = 400.f;

	/** Slot angle off the leader's back when beside them (90 is level with their shoulder). */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0", ClampMax = "90"))
	float SideBySideAngle = 75.f;

	/** Slot distance when beside them. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float SideBySideDistance = 170.f;

	/**
	 * Free width below which it is a corridor: the slot goes straight behind (angle 0) and a little
	 * closer, 0.85 x FormationDistance. 0 = never single file.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float CorridorWidth = 250.f;

	/** Seconds the slot takes to blend between normal, beside and single file. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Formation", meta = (ClampMin = "0"))
	float FormationBlendTime = 1.f;

	/**
	 * While the leader strolls (moving, slower than SideBySideMaxLeaderSpeed), catching up is a
	 * quickened step, not a sprint: pace capped at this many times her walk speed. Measured in game
	 * without it: a leader doubling back at 150 left her 5-8 m behind and she ran at 400-680. 0 off.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float StrollCatchUpFactor = 1.6f;

	/** Further from the leader than this she may still run, stroll or not: that is being left behind. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Speed", meta = (ClampMin = "0"))
	float StrollCatchUpMaxGap = 1100.f;

	/*------------------------------ Internal ----------------------------------*/

	UPROPERTY()
	bool bMoving = false;

	/** A start has been decided and is waiting out the reaction time. */
	UPROPERTY()
	bool bReacting = false;

	UPROPERTY()
	float ReactionCountdown = 0.f;

	/** Seconds left holding the step-aside goal after the leader last walked at her. */
	UPROPERTY()
	float StepAsideCountdown = 0.f;

	UPROPERTY()
	FVector StepAsideGoal = FVector::ZeroVector;

	/** Smoothed free width across the leader's heading, and when to measure it next. */
	UPROPERTY()
	float SmoothedLeaderWidth = -1.f;

	UPROPERTY()
	float WidthCheckCountdown = 0.f;

	/** Formation angle and distance actually in use, blended towards the shape the space asks for. */
	UPROPERTY()
	float BlendedFormationAngle = -1.f;

	UPROPERTY()
	float BlendedFormationDistance = -1.f;

	/** 0..1 how hard it is currently committing to the move. Ramps, never jumps. */
	UPROPERTY()
	float Intent = 0.f;

	UPROPERTY()
	FVector SmoothedLeaderForward = FVector::ZeroVector;

	UPROPERTY()
	bool bPathing = false;

	UPROPERTY()
	bool bWayIsClear = true;

	/** Furthest point down the line to the goal that is walkable, and how far that is. */
	UPROPERTY()
	FVector ClearPoint = FVector::ZeroVector;

	UPROPERTY()
	float ClearReach = 0.f;

	UPROPERTY()
	float TimeSinceObstacleCheck = 0.f;

	UPROPERTY()
	float TraceCountdown = 0.f;

	/** Last steering mode that was traced, so level 2 can report the moment it flips. */
	UPROPERTY()
	bool bTracedSteering = true;

	UPROPERTY()
	float CurrentStopDistance = 0.f;

	UPROPERTY()
	float TimeSinceSideSwap = 0.f;

	UPROPERTY()
	float TimeSinceRepath = 0.f;

	UPROPERTY()
	FVector LastGoal = FVector::ZeroVector;

	UPROPERTY()
	float CachedMaxWalkSpeed = 0.f;

	/** Rate-limited speed actually handed to the movement component. */
	UPROPERTY()
	float CommandedSpeed = 0.f;

	/** The formation side as a continuous value, so a swap sweeps across instead of teleporting. */
	UPROPERTY()
	float SmoothedSide = 0.f;

	UPROPERTY()
	FVector SmoothedSteerDirection = FVector::ZeroVector;

	/** The parameter dump goes out on the first tick, not on entry: see the comment there. */
	UPROPERTY()
	bool bLoggedParams = false;

	/**
	 * Whether she is standing on a chosen spot this frame. The spot itself, and the clock that
	 * decides when she may choose one, live on AViperCompanionAIController: a task's instance data
	 * is zeroed every time the tree re-selects its states, and that had her re-rolling the bearing
	 * every few seconds.
	 */
	UPROPERTY()
	bool bHasPlantedSpot = false;
};

/**
 * Keeps the companion near its leader the way a person would: a slot over one shoulder instead of dead centre
 * behind, a start/stop band with random slack so it never stutters in place, speed matched to the leader,
 * and the goal pushed ahead by the leader velocity so it cuts corners.
 * Never finishes: leave the state with transitions/events.
 */
USTRUCT(meta = (DisplayName = "Viper Follow Leader", Category = "Viper|Companion"))
struct FViperFollowLeaderTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperFollowLeaderTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/***********************************************************************/
/*                        TASK: VIPER LOOK AT                          */
/***********************************************************************/

USTRUCT()
struct FViperLookAtTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Who to look at. Bind it to the Leader output of the evaluator. Overrides TargetLocation. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> Target = nullptr;

	/** World point to look at when Target is empty. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector TargetLocation = FVector::ZeroVector;

	/** 0..1 how hard it commits to the gaze. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Also turns the whole body to face the target (AI focus), not just the head. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bTurnBody = false;

	/** Beyond this the companion does not bother looking (0 = no limit). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float MaxLookAtDistance = 1800.f;

	/**
	 * Nobody stares without blinking. With this on the gaze holds for a random spell, glances away,
	 * then comes back. This is the detail that stops it reading as a camera on a tripod.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact")
	bool bBreakEyeContact = true;

	/**
	 * How long each spell of eye contact lasts. Drawn from a normal distribution (MeanHoldTime,
	 * HoldTimeSpread) and clamped to [MinHoldTime, MaxHoldTime]: measured mutual gaze mostly runs one
	 * to two seconds with a preferred length near three, and a flat spread between the limits made
	 * the long stares as common as the ordinary ones.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0.1"))
	float MinHoldTime = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0.1"))
	float MaxHoldTime = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0.1"))
	float MeanHoldTime = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0"))
	float HoldTimeSpread = 0.6f;

	/** The look-away is brief: a moment off and straight back, not a second look at something else. */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0.1"))
	float MinGlanceAwayTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0.1"))
	float MaxGlanceAwayTime = 1.f;

	/** Most degrees to the side the glance drifts while looking away (at least 40 per cent of it). */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0", ClampMax = "90"))
	float GlanceAwayAngle = 25.f;

	/** Degrees the glance also drops. Looking away slightly down reads as thinking; level reads as distracted. */
	UPROPERTY(EditAnywhere, Category = "Parameter|EyeContact", meta = (EditCondition = "bBreakEyeContact", ClampMin = "0", ClampMax = "45"))
	float GlanceDownAngle = 8.f;

	/** Succeeds after this many seconds (0 = runs until the state is left). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float Duration = 0.f;

	/*------------------------------ Internal ----------------------------------*/

	UPROPERTY()
	float ElapsedTime = 0.f;

	UPROPERTY()
	bool bGlancingAway = false;

	UPROPERTY()
	float PhaseCountdown = 0.f;

	UPROPERTY()
	FVector GlanceOffsetDirection = FVector::ZeroVector;

	/** Movement flags saved while bTurnBody drives the body rotation, restored on exit. */
	UPROPERTY()
	bool bBodyRotationOverridden = false;

	UPROPERTY()
	bool bCachedOrientRotationToMovement = false;

	UPROPERTY()
	bool bCachedUseControllerRotationYaw = false;
};

/**
 * Makes the companion look at an actor or a point: head and spine via the character gaze (replicated),
 * optionally the whole body via AI focus. Breaks and re-establishes eye contact on its own.
 */
USTRUCT(meta = (DisplayName = "Viper Look At", Category = "Viper|Companion"))
struct FViperLookAtTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperLookAtTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/***********************************************************************/
/*                         TASK: VIPER WANDER                          */
/***********************************************************************/

USTRUCT()
struct FViperWanderTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Optional. When set, the wander point is kept within MaxLeashFromLeader of the leader. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> Leader = nullptr;

	/** Radius around the companion the point is picked from. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1"))
	float WanderRadius = 700.f;

	/** Reject points closer than this, otherwise it shuffles on the spot. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float MinWanderDistance = 250.f;

	/** Never wander further than this from the leader, or from LeashCentre when that is set (0 = off the leash). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float MaxLeashFromLeader = 1400.f;

	/**
	 * Centre of the area she is free to move about in. Bind it to the LeaderAnchor output of the
	 * evaluator - where the leader was when they stopped. Points are then picked around that
	 * rather than around her own feet, which is the difference between wandering inside one area
	 * and a random walk that drifts to wherever the last step happened to leave her. Left at zero
	 * it falls back to the old behaviour: her own position, leashed to the leader.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector LeashCentre = FVector::ZeroVector;

	/**
	 * Reject points this close to the leader. Her own business does not include walking through
	 * them, and an area centred on where they stand offers plenty of points that are the ground
	 * they are standing on.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float MinDistanceFromLeader = 200.f;

	/**
	 * Look for something to examine before settling for bare ground. Actors implementing
	 * IViperInteractableInterface inside the area are what she walks over to; only when there are
	 * none does she fall back on choosing open ground. This is the difference between a detective
	 * looking at things and a pawn shuffling about a circle.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bPreferInteractables = true;

	/** How far short of a find she stops. Walking onto a thing is not looking at it. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float StandOffFromFind = 140.f;

	/**
	 * How far ahead the ground is checked when there is nothing to examine. Of the candidate spots,
	 * the one with the longest clear run still ahead of it wins, and she stands looking down that
	 * run - so she ends up facing somewhere one could keep going, not a wall.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float OpenGroundReach = 1400.f;

	/** Same idea as the follow task's: prefer to stand looking onwards, not back the way she came. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0", ClampMax = "1"))
	float LookForwardBias = 0.75f;

	/** Speed while wandering. Low: it is not in a hurry, it is curious. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float WanderSpeed = 180.f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1"))
	float AcceptanceRadius = 60.f;

	/** Looks where it is going. Turn it off if another Look At task in the state owns the gaze. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bLookAtDestination = true;

	/** Gives up and succeeds after this long, so a blocked path never wedges the state. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1"))
	float MaxWanderTime = 12.f;

	/**
	 * Stands at the spot it picked for this long before the task ends. Walking somewhere and turning
	 * straight back is an errand; stopping to look at what drew it there is curiosity.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float DwellTime = 2.5f;

	/**
	 * Gaze-leading. On arriving at something to examine with the leader in sight, she looks at it,
	 * glances at the leader, and looks back at it: the way a person points a thing out with their
	 * eyes. Needs bLookAtDestination, since that is what owns her head here.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|GazeLead", meta = (EditCondition = "bLookAtDestination"))
	bool bGazeLeadOnFinds = true;

	/** Only when the leader is this close (and in sight). Further off nobody would catch the glance. */
	UPROPERTY(EditAnywhere, Category = "Parameter|GazeLead", meta = (EditCondition = "bGazeLeadOnFinds", ClampMin = "0"))
	float GazeLeadMaxDistance = 1200.f;

	/** Seconds on the find before the glance, +/-20 per cent (0.8-1.2 s by default). */
	UPROPERTY(EditAnywhere, Category = "Parameter|GazeLead", meta = (EditCondition = "bGazeLeadOnFinds", ClampMin = "0"))
	float GazeLeadFindLookTime = 1.f;

	/** Seconds the glance at the leader lasts, +/-20 per cent (0.6-0.9 s by default). */
	UPROPERTY(EditAnywhere, Category = "Parameter|GazeLead", meta = (EditCondition = "bGazeLeadOnFinds", ClampMin = "0"))
	float GazeLeadLeaderLookTime = 0.75f;

	/**
	 * Steps out of the way when the leader walks at her, as the follow task does: the leader may
	 * pace about the area she is roaming in, and she used to stay planted in their path. Distance
	 * ahead of them she reacts within (0 = never).
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0"))
	float StepAsideDistance = 250.f;

	/** Half-angle of the cone ahead of the leader that counts as their path. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0", ClampMax = "90"))
	float LeaderPathConeAngle = 35.f;

	/** Below this the leader is shuffling, not walking anywhere. Same value as the follow task. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0"))
	float StepAsideMinLeaderSpeed = 20.f;

	/** How far to the side of the leader's line she steps to: past their personal space. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0"))
	float StepAsideClearance = 180.f;

	/** Pace while stepping aside. The stroll pace is far too slow to get out of the way of someone walking at her. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0"))
	float StepAsideSpeed = 320.f;

	/**
	 * While the leader is walking, never pick a spot in the cone ahead of them out to this far.
	 * Walking into someone's path to stand there is the same mistake as not leaving it.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter|Space", meta = (ClampMin = "0"))
	float AvoidLeaderPathLength = 500.f;

	/*------------------------------ Internal ----------------------------------*/

	UPROPERTY()
	bool bArrived = false;

	/** This leg ended at an interactable (the FIND case), not at open ground. */
	UPROPERTY()
	bool bAtFind = false;

	/** 0 none, 1 looking at the find, 2 glancing at the leader, 3 back on the find for good. */
	UPROPERTY()
	int32 GazeLeadPhase = 0;

	UPROPERTY()
	float GazeLeadCountdown = 0.f;

	UPROPERTY()
	float DwellCountdown = 0.f;

	UPROPERTY()
	float ElapsedTime = 0.f;

	/** Above zero while stepping out of the leader's way; held a moment so she does not step straight back. */
	UPROPERTY()
	float StepAsideCountdown = 0.f;

	UPROPERTY()
	FVector StepAsideGoal = FVector::ZeroVector;

	UPROPERTY()
	FVector Destination = FVector::ZeroVector;

	/** What she turns to look at on arrival: the find, or down the open ground she chose. */
	UPROPERTY()
	FVector LookTarget = FVector::ZeroVector;

	UPROPERTY()
	float CachedMaxWalkSpeed = 0.f;
};

/**
 * Sends the companion off to a random reachable point near itself, at a stroll, on a leash from the leader.
 * Succeeds on arrival or on timeout, fails when the navmesh offers nothing.
 */
USTRUCT(meta = (DisplayName = "Viper Wander", Category = "Viper|Companion"))
struct FViperWanderTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperWanderTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/***********************************************************************/
/*                       TASK: VIPER IDLE BREAK                        */
/***********************************************************************/

USTRUCT()
struct FViperIdleBreakTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Stands there for a random spell inside this range. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1"))
	float MinDuration = 2.5f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1"))
	float MaxDuration = 7.f;

	/** Glances at random things around it while it waits. This is what reads as "being somewhere". */
	UPROPERTY(EditAnywhere, Category = "Parameter|Glance")
	bool bLookAround = true;

	UPROPERTY(EditAnywhere, Category = "Parameter|Glance", meta = (EditCondition = "bLookAround", ClampMin = "0.1"))
	float MinGlanceInterval = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameter|Glance", meta = (EditCondition = "bLookAround", ClampMin = "0.1"))
	float MaxGlanceInterval = 2.8f;

	/** How far out the things it glances at are. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Glance", meta = (EditCondition = "bLookAround", ClampMin = "1"))
	float GlanceRadius = 900.f;

	/** Half-angle of the arc it glances across. 180 lets it look over its shoulder. */
	UPROPERTY(EditAnywhere, Category = "Parameter|Glance", meta = (EditCondition = "bLookAround", ClampMin = "0", ClampMax = "180"))
	float GlanceArc = 110.f;

	/** Report the break to the controller, so TimeSinceLastIdleBreak paces the next one. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRegisterAsIdleBreak = true;

	/**
	 * Seconds it takes to roll to a halt on entering, instead of dropping the input in one frame.
	 * 0 cuts the movement dead, which is what reads as a machine switching off.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float StopTime = 0.8f;

	/*------------------------------ Internal ----------------------------------*/

	UPROPERTY()
	float Countdown = 0.f;

	UPROPERTY()
	float GlanceCountdown = 0.f;

	UPROPERTY()
	float CachedBrakingDeceleration = 0.f;
};

/**
 * The companion stops and just exists for a few seconds: no movement, eyes wandering over whatever is around.
 * Succeeds when the spell is over.
 */
USTRUCT(meta = (DisplayName = "Viper Idle Break", Category = "Viper|Companion"))
struct FViperIdleBreakTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperIdleBreakTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/***********************************************************************/
/*                          TASK: VIPER SPEAK                          */
/***********************************************************************/

USTRUCT()
struct FViperSpeakTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Line to play. Only the leader hears it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TSoftObjectPtr<USoundBase> VoiceLine;

	/**
	 * Length of the line in seconds. The server never loads the sound (it is a soft ref), so it cannot
	 * measure this: it drives IsSpeaking and the task duration.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float Duration = 3.f;

	/** Keeps running until the line is over. Otherwise succeeds straight away. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bWaitForEnd = true;

	/*------------------------------ Internal ----------------------------------*/

	UPROPERTY()
	float ElapsedTime = 0.f;
};

/**
 * Plays one of the companion voice lines for its leader and flags it as speaking.
 * Put a Viper Look At next to it in the same state and it will talk to your face.
 */
USTRUCT(meta = (DisplayName = "Viper Speak", Category = "Viper|Companion"))
struct FViperSpeakTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperSpeakTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/***********************************************************************/
/*                    TASK: VIPER ACTIVATE ABILITY                     */
/***********************************************************************/

USTRUCT()
struct FViperActivateAbilityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** Activates the pawn abilities that have this tag (e.g. Ability.Companion.Wave). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag AbilityTag;

	/** Keeps running until the ability ends. Otherwise succeeds right after activating it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bWaitForAbilityEnd = true;

	/** Safety limit when waiting for the ability to end (0 = no limit). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bWaitForAbilityEnd", ClampMin = "0"))
	float MaxWaitTime = 5.f;

	UPROPERTY()
	float ElapsedTime = 0.f;
};

/** Activates a GAS ability of the controlled pawn by tag (e.g. an animation). Fails if it cannot be activated (cooldown, cost, blocked...). */
USTRUCT(meta = (DisplayName = "Viper Activate Ability", Category = "Viper|Companion"))
struct FViperActivateAbilityTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FViperActivateAbilityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
