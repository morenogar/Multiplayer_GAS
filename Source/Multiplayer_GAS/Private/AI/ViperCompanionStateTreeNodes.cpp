// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/ViperCompanionStateTreeNodes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "AI/ViperCompanionCharacter.h"
#include "AI/ViperCompanionTuning.h"
#include "Interactables/ViperInteractableInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogViperCompanion);

static TAutoConsoleVariable<int32> CVarViperCompanionDebug(
	TEXT("Viper.Companion.Debug"),
	0,
	TEXT("Companion movement trace.\n")
	TEXT("  0: off\n")
	TEXT("  1: a summary line a few times a second\n")
	TEXT("  2: the same, plus every mode change the moment it happens"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarViperCompanionDebugInterval(
	TEXT("Viper.Companion.DebugInterval"),
	0.25f,
	TEXT("Seconds between companion trace lines at debug level 1 or higher."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarViperCompanionCsv(
	TEXT("Viper.Companion.Csv"),
	0,
	TEXT("Writes one csv row per sample with the whole pair on it (leader and companion position, yaw,\n")
	TEXT("speed, the gap, the aim test and what the follow task is doing). Separate from Debug because\n")
	TEXT("the readable trace is for reading and this is for plotting: they want different rates."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarViperCompanionCsvInterval(
	TEXT("Viper.Companion.CsvInterval"),
	0.0333f,
	TEXT("Seconds between csv rows. The default is about 30 a second, enough to replay the motion."),
	ECVF_Default);

namespace ViperCompanionStateTree
{
	static int32 DebugLevel()
	{
		return CVarViperCompanionDebug.GetValueOnAnyThread();
	}

	/** True once every DebugInterval seconds, so a trace reads like a timeline instead of a wall. */
	static bool ShouldTrace(float& Countdown, float DeltaTime)
	{
		if (DebugLevel() <= 0)
		{
			return false;
		}

		Countdown -= DeltaTime;
		if (Countdown > 0.f)
		{
			return false;
		}

		Countdown = FMath::Max(0.05f, CVarViperCompanionDebugInterval.GetValueOnAnyThread());
		return true;
	}

	/** True once every CsvInterval seconds while the csv trace is on. */
	static bool ShouldSampleCsv(float& Countdown, float DeltaTime)
	{
		if (CVarViperCompanionCsv.GetValueOnAnyThread() <= 0)
		{
			return false;
		}

		Countdown -= DeltaTime;
		if (Countdown > 0.f)
		{
			return false;
		}

		Countdown = FMath::Max(0.01f, CVarViperCompanionCsvInterval.GetValueOnAnyThread());
		return true;
	}

	/**
	 * Turns a standing pawn to face a point, and only while nothing else owns its yaw.
	 * bOrientRotationToMovement does nothing without acceleration, so writing the rotation while
	 * stopped does not fight it. The look-at task clears that flag when it takes the body over,
	 * and that is the signal here to keep hands off.
	 */
	static void TurnTowardsWhileIdle(APawn& Pawn, const FVector& Target, float TurnRate, float DeltaTime)
	{
		const UCharacterMovementComponent* Movement = Pawn.FindComponentByClass<UCharacterMovementComponent>();
		if (!Movement || !Movement->bOrientRotationToMovement)
		{
			return;
		}

		// Still carrying speed: let the walk finish before turning the shoulders.
		if (Movement->Velocity.SizeSquared2D() > FMath::Square(15.f))
		{
			return;
		}

		const FVector ToTarget = (Target - Pawn.GetActorLocation()).GetSafeNormal2D();
		if (ToTarget.IsNearlyZero())
		{
			return;
		}

		const FRotator Current = Pawn.GetActorRotation();
		const FRotator FlatCurrent(0.f, Current.Yaw, 0.f);
		const FRotator Desired(0.f, ToTarget.Rotation().Yaw, 0.f);
		const FRotator Next = FMath::RInterpConstantTo(FlatCurrent, Desired, DeltaTime, TurnRate);
		Pawn.SetActorRotation(FRotator(Current.Pitch, Next.Yaw, Current.Roll));
	}

	static UAbilitySystemComponent* GetPawnAbilitySystem(const AAIController* AIController)
	{
		return AIController ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AIController->GetPawn()) : nullptr;
	}

	static AViperCompanionCharacter* GetCompanionCharacter(const AAIController* AIController)
	{
		return AIController ? Cast<AViperCompanionCharacter>(AIController->GetPawn()) : nullptr;
	}

	static UCharacterMovementComponent* GetMovement(const AAIController* AIController)
	{
		const APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
	}

	/**
	 * Lets the live tuning object win over the parameters baked into ST_Companion, so a walk can be
	 * judged while it is happening instead of through edit, compile, restart. Matched by reflection
	 * on the real property names, which means adding a parameter to the task needs nothing here,
	 * and names the task does not have are tried on the movement component next.
	 */
	static void ApplyTuning(FViperFollowLeaderTaskInstanceData& Data, UCharacterMovementComponent* Movement)
	{
		const UViperCompanionTuning* Tuning = UViperCompanionTuning::Get();
		if (!Tuning || !Tuning->bEnabled || Tuning->Overrides.IsEmpty())
		{
			return;
		}

		UStruct* TaskStruct = FViperFollowLeaderTaskInstanceData::StaticStruct();
		for (const TPair<FName, float>& Override : Tuning->Overrides)
		{
			if (FProperty* Property = TaskStruct->FindPropertyByName(Override.Key))
			{
				if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
				{
					FloatProperty->SetPropertyValue_InContainer(&Data, Override.Value);
					continue;
				}
				if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
				{
					BoolProperty->SetPropertyValue_InContainer(&Data, Override.Value != 0.f);
					continue;
				}
			}

			if (!Movement)
			{
				continue;
			}

			// An FRotator is not a float, and the only part of it that matters here is the yaw.
			if (Override.Key == TEXT("RotationRate"))
			{
				Movement->RotationRate.Yaw = Override.Value;
				continue;
			}

			if (FProperty* Property = Movement->GetClass()->FindPropertyByName(Override.Key))
			{
				if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
				{
					FloatProperty->SetPropertyValue_InContainer(Movement, Override.Value);
				}
				else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
				{
					BoolProperty->SetPropertyValue_InContainer(Movement, Override.Value != 0.f);
				}
			}
		}
	}

	/** Eye-ish height point, used to aim gazes without needing the mesh. */
	static FVector GetEyeApprox(const APawn& Pawn)
	{
		return Pawn.GetActorLocation() + FVector(0.f, 0.f, Pawn.BaseEyeHeight);
	}

	/** Snaps a point onto the navmesh. Returns false (and passes the point through) when there is no navmesh. */
	static bool ProjectToNavigation(UWorld* World, const FVector& InPoint, FVector& OutPoint)
	{
		if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation NavLocation;
			if (NavSystem->ProjectPointToNavigation(InPoint, NavLocation))
			{
				OutPoint = NavLocation.Location;
				return true;
			}
		}
		OutPoint = InPoint;
		return false;
	}

	/**
	 * How far along the straight line to the goal the companion can actually walk, and whether that
	 * is the whole way. The question used to be yes or no, and a no sent it to the navmesh for a
	 * path. On a map whose walkable strip is narrower than the formation slot is wide, a no is most
	 * of the time - measured at 75 to 89 per cent of a walk - and every path request re-aims it at
	 * the first corner of a new path and brakes into it again. That is where the surging came from:
	 * the steps spent steering showed 2 pace changes per ten seconds, the steps spent pathing 62.
	 * Steering at the furthest reachable point keeps the motion continuous and still rounds whatever
	 * is in the way, because the point slides along the edge as the companion moves.
	 */
	static bool FurthestClearPoint(UWorld* World, const FVector& From, const FVector& To, FVector& OutPoint)
	{
		OutPoint = To;

		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavSystem)
		{
			return true;
		}

		FVector HitLocation;
		if (!NavSystem->NavigationRaycast(World, From, To, HitLocation))
		{
			return true;
		}

		const FVector Along = HitLocation - From;
		const float Reach = Along.Size2D();
		if (Reach <= KINDA_SMALL_NUMBER)
		{
			// Blocked where it stands, which is also what a raycast off the navmesh answers. Hand back
			// its own position rather than leaving the goal in OutPoint, or the caller measures the
			// reach to the goal, decides the whole way is walkable and steers straight at it.
			OutPoint = From;
			return false;
		}

		// Stand off the edge it just found instead of walking into it.
		OutPoint = From + Along.GetSafeNormal() * FMath::Max(0.f, Reach - 40.f);
		return false;
	}

	/**
	 * The most open direction from a point, and how far the ground keeps going that way. Used to
	 * choose where to stand when there is nothing to examine: a spot with a long run ahead of it
	 * is a spot worth standing at, and the direction is what she then looks down.
	 */
	static float OpenestDirection(UWorld* World, const FVector& From, float MaxReach,
		float PreferYaw, float Bias, FVector& OutDirection)
	{
		OutDirection = FVector::ForwardVector;
		float BestReach = 0.f;
		float BestScore = 0.f;

		for (int32 Step = 0; Step < 12; ++Step)
		{
			const float Yaw = Step * 30.f;
			const FVector Direction = FRotator(0.f, Yaw, 0.f).RotateVector(FVector::ForwardVector);
			FVector ClearPoint;
			const bool bClear = FurthestClearPoint(World, From, From + Direction * MaxReach, ClearPoint);
			const float Reach = bClear ? MaxReach : FVector::Dist2D(From, ClearPoint);

			// A way out behind her has to be this much more open to win: with open ground both ways
			// the plain furthest rule had no opinion and would turn her round on arrival.
			float Weight = 1.f;
			if (Bias > 0.f)
			{
				const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, PreferYaw));
				const float Ahead = (1.f + FMath::Cos(FMath::DegreesToRadians(Off))) * 0.5f;
				Weight = (1.f - Bias) + Bias * Ahead;
			}

			const float Score = Reach * Weight;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestReach = Reach;
				OutDirection = Direction;
			}
		}
		return BestReach;
	}

	/**
	 * Everything in the world that can be interacted with, within a radius of a point, nearest
	 * first. Excluded: the companion herself and her leader, both of which are interactable and
	 * neither of which is a thing to go and examine.
	 */
	static void GatherFinds(UWorld* World, const FVector& Centre, float Radius,
		const AActor* Pawn, const AActor* Leader, TArray<AActor*>& OutFinds)
	{
		OutFinds.Reset();
		if (!World)
		{
			return;
		}

		TArray<AActor*> Candidates;
		UGameplayStatics::GetAllActorsWithInterface(World, UViperInteractableInterface::StaticClass(), Candidates);
		for (AActor* Candidate : Candidates)
		{
			if (!Candidate || Candidate == Pawn || Candidate == Leader)
			{
				continue;
			}
			if (FVector::Dist2D(Candidate->GetActorLocation(), Centre) <= Radius)
			{
				OutFinds.Add(Candidate);
			}
		}

		const FVector From = Pawn ? Pawn->GetActorLocation() : Centre;
		OutFinds.Sort([From](const AActor& A, const AActor& B)
		{
			return FVector::DistSquared2D(A.GetActorLocation(), From)
				 < FVector::DistSquared2D(B.GetActorLocation(), From);
		});
	}

	/**
	 * A sample from a normal distribution (Box-Muller). Human timings - how long a gaze is held,
	 * how long it takes to react - cluster round a typical value with the odd long or short one;
	 * a flat random range makes the extremes as common as the middle, which reads as erratic.
	 */
	static float RandNormal(float Mean, float Spread)
	{
		const float U1 = FMath::Max(FMath::FRand(), KINDA_SMALL_NUMBER);
		const float U2 = FMath::FRand();
		const float Z = FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
		return Mean + Z * Spread;
	}

	/**
	 * Free walkable width across a point, measured along Axis with one navmesh raycast each way, each
	 * at most MaxReach long. Navmesh is already shrunk by the agent radius, so the caller adds its
	 * own body width back to get something comparable to the physical gap. No navmesh: unlimited.
	 */
	static float FreeWidthAcross(UWorld* World, const FVector& Point, const FVector& Axis, float MaxReach)
	{
		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const FVector Across = Axis.GetSafeNormal2D();
		if (!NavSystem || Across.IsNearlyZero())
		{
			return 2.f * MaxReach;
		}

		float Width = 0.f;
		for (const float Sign : { 1.f, -1.f })
		{
			FVector Hit;
			const FVector End = Point + Across * (Sign * MaxReach);
			Width += NavSystem->NavigationRaycast(World, Point, End, Hit) ? FVector::Dist2D(Point, Hit) : MaxReach;
		}
		return Width;
	}

	/** True when Point lies inside the cone ahead of the leader, within Length of them. */
	static bool InLeaderPathCone(const FVector& LeaderLocation, const FVector& LeaderForward,
		const FVector& Point, float HalfAngle, float Length)
	{
		const FVector ToPoint = FVector(Point.X - LeaderLocation.X, Point.Y - LeaderLocation.Y, 0.f);
		const float Along = FVector::DotProduct(ToPoint, LeaderForward.GetSafeNormal2D());
		if (Along <= 0.f || ToPoint.Size2D() > Length)
		{
			return false;
		}
		return Along >= ToPoint.Size2D() * FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	}

	/**
	 * Somewhere to stand near a leader who has stopped: a bearing off their back, rolled so she is
	 * not forever directly behind them, at least a personal space out, on the navmesh and with
	 * nothing between it and them. Standing with someone means being visible to them.
	 * Not in the way, either: never in the cone ahead of them (where they will walk next) and never
	 * somewhere narrow like a doorway, where standing still blocks the only way through.
	 */
	static bool PickStandingSpot(UWorld* World, const FVector& LeaderLocation, const FVector& LeaderForward,
		float BearingSpread, float PersonalSpace, float FormationDistance,
		float PathConeAngle, float PathConeLength, float MinWidth, float BodyRadius, FVector& OutSpot)
	{
		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const float Spread = FMath::Max(0.f, BearingSpread);

		for (int32 Attempt = 0; Attempt < 16; ++Attempt)
		{
			const float Side = FMath::RandBool() ? 1.f : -1.f;
			const float Offset = FMath::FRand() * Spread;
			const float Reach = FMath::Max(PersonalSpace + 50.f,
				FMath::Lerp(FormationDistance * 0.7f, FormationDistance, FMath::FRand()));
			FVector Candidate = LeaderLocation
				+ FRotator(0.f, Side * Offset, 0.f).RotateVector(-LeaderForward) * Reach;

			if (NavSystem)
			{
				FNavLocation Projected;
				if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(120.f, 120.f, 220.f)))
				{
					continue;
				}
				// A projection that drags the spot somewhere else is not the spot that was chosen.
				if (FVector::Dist2D(Projected.Location, Candidate) > 100.f)
				{
					continue;
				}
				Candidate = Projected.Location;
			}

			if (PersonalSpace > 0.f && FVector::Dist2D(Candidate, LeaderLocation) < PersonalSpace)
			{
				continue;
			}

			// Where they are about to walk. Only reachable with a spread past 180 minus the cone,
			// but the projection above can also slide a spot round into it.
			if (PathConeLength > 0.f
				&& InLeaderPathCone(LeaderLocation, LeaderForward, Candidate, PathConeAngle, PathConeLength))
			{
				continue;
			}

			// A doorway. Measured across the bearing from the leader, which is the axis a doorway
			// between the two of them is narrow on.
			if (MinWidth > 0.f)
			{
				const FVector Across = FVector::CrossProduct(FVector::UpVector, Candidate - LeaderLocation);
				if (FreeWidthAcross(World, Candidate, Across, MinWidth) + 2.f * BodyRadius < MinWidth)
				{
					continue;
				}
			}

			FVector ClearPoint;
			if (!FurthestClearPoint(World, LeaderLocation, Candidate, ClearPoint))
			{
				continue;
			}

			OutSpot = Candidate;
			return true;
		}

		return false;
	}

	/**
	 * What she should be looking at while standing about: the nearest thing worth examining that
	 * she can see, and failing that a point down the most open run of ground. False when there is
	 * nothing better to look at than the leader.
	 */
	static bool ChooseStandLook(UWorld* World, const APawn& Pawn, const AActor* Leader,
		float FindRange, float OpenReach, float ForwardBias, FVector& OutTarget)
	{
		const FVector From = Pawn.GetActorLocation();

		if (FindRange > 0.f)
		{
			TArray<AActor*> Finds;
			GatherFinds(World, From, FindRange, &Pawn, Leader, Finds);   // nearest first
			for (const AActor* Find : Finds)
			{
				FVector ClearPoint;
				if (!FurthestClearPoint(World, From, Find->GetActorLocation(), ClearPoint))
				{
					continue;      // something in the way: not what she is looking at
				}
				OutTarget = Find->GetActorLocation();
				return true;
			}
		}

		// Her own yaw is still the heading she walked in on: the gaze is chosen on the frame she
		// stops, and the movement component had been turning her to face her velocity.
		FVector OpenDirection;
		const float Reach = OpenestDirection(World, From, OpenReach,
			Pawn.GetActorRotation().Yaw, ForwardBias, OpenDirection);
		if (Reach < 200.f)
		{
			return false;          // nowhere runs on: the leader is as good a thing to face as any
		}

		OutTarget = From + OpenDirection * FMath::Clamp(Reach, 300.f, 900.f);
		return true;
	}

	/** True when a straight walk from From to To stays on the navmesh, so no path is needed to get there. */
	static bool IsWayClear(UWorld* World, const FVector& From, const FVector& To)
	{
		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavSystem)
		{
			// No navmesh at all: steering straight is the only thing left that works.
			return true;
		}

		FVector HitLocation;
		return !NavSystem->NavigationRaycast(World, From, To, HitLocation);
	}

	/**
	 * Comes to a halt over StopTime seconds instead of dropping the input dead. It sizes the braking
	 * deceleration to the speed it is actually carrying, so the stop takes the same time from a stroll
	 * or from a run. Returns the deceleration it replaced, for the caller to put back.
	 */
	static float BeginSoftStop(AAIController& Controller, float StopTime)
	{
		float PreviousDeceleration = 0.f;

		if (UCharacterMovementComponent* Movement = GetMovement(&Controller))
		{
			PreviousDeceleration = Movement->BrakingDecelerationWalking;

			const float Speed = Movement->Velocity.Size2D();
			if (StopTime > KINDA_SMALL_NUMBER && Speed > KINDA_SMALL_NUMBER)
			{
				Movement->BrakingDecelerationWalking = FMath::Max(50.f, Speed / StopTime);
			}
		}

		Controller.StopMovement();
		return PreviousDeceleration;
	}

	static void RestoreBraking(AAIController& Controller, float PreviousDeceleration)
	{
		if (PreviousDeceleration <= 0.f)
		{
			return;
		}

		if (UCharacterMovementComponent* Movement = GetMovement(&Controller))
		{
			Movement->BrakingDecelerationWalking = PreviousDeceleration;
		}
	}

	static void TeleportNearLeader(AAIController& Controller, APawn& Pawn, const AActor& Leader, float Offset)
	{
		FVector Destination = Leader.GetActorLocation() - Leader.GetActorForwardVector() * Offset;

		FVector Projected;
		if (ProjectToNavigation(Pawn.GetWorld(), Destination, Projected))
		{
			// Navmesh points are on the floor: lift the pawn so its capsule does not start inside the ground.
			Destination = Projected + FVector(0.f, 0.f, Pawn.GetDefaultHalfHeight());
		}

		Controller.StopMovement();
		Pawn.TeleportTo(Destination, Leader.GetActorRotation());
	}

	static void UpdateEvaluatorData(FViperCompanionEvaluatorInstanceData& Data, float DeltaTime)
	{
		const AViperCompanionAIController* Controller = Cast<AViperCompanionAIController>(Data.AIController);
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		APawn* LeaderPawn = Controller ? Controller->GetLeaderPawn() : nullptr;

		// Re-roll on its own clock: a random value that holds for a while keeps states from flickering.
		Data.RandomRerollCountdown -= DeltaTime;
		if (Data.RandomRerollCountdown <= 0.f)
		{
			Data.RandomValue = FMath::FRand();
			Data.RandomRerollCountdown = FMath::Max(0.1f, Data.RandomRerollInterval);
		}

		// Only the outputs are rewritten every tick. Parameters and internal state are left alone
		// (the old version replaced the whole struct, which would wipe the parameters above).
		Data.Leader = LeaderPawn;
		Data.bHasLeader = LeaderPawn != nullptr;
		Data.DistanceToLeader = 0.f;
		Data.Stance = EViperCompanionStance::Follow;
		Data.HoldLocation = FVector::ZeroVector;
		Data.DistanceToHoldLocation = 0.f;
		Data.TimeSinceLastVoice = 0.f;
		Data.bIsSpeaking = false;
		Data.LeaderSpeed = 0.f;
		Data.bLeaderIsMoving = false;
		Data.bLeaderLooksAtMe = false;
		Data.LeaderAimErrorDegrees = 180.f;
		Data.EyeContactThresholdDegrees = 0.f;
		Data.bHasLineOfSightToLeader = false;
		Data.TimeSinceLastIdleBreak = 0.f;
		Data.bLeaderNoticesMe = false;
		Data.bLeaderStudying = false;
		Data.DistanceLeaderFromAnchor = 0.f;
		// TimeLookedAt, TimeLeaderStayingPut, LeaderAnchor, bLeaderSettled and the two internals are
		// accumulators and latches, deliberately not cleared here: they are what the tree reads to
		// tell a look from a glance, and a leader who has settled somewhere from one who merely
		// paused between strides.

		if (!Controller || !Pawn)
		{
			return;
		}

		Data.Stance = Controller->GetStance();
		Data.HoldLocation = Controller->GetHoldLocation();
		Data.TimeSinceLastVoice = Controller->GetTimeSinceLastVoice();
		Data.bIsSpeaking = Controller->IsSpeaking();
		Data.TimeSinceLastIdleBreak = Controller->GetTimeSinceLastIdleBreak();

		const FVector PawnLocation = Pawn->GetActorLocation();

		if (Data.Stance == EViperCompanionStance::HoldPosition)
		{
			Data.DistanceToHoldLocation = FVector::Dist(PawnLocation, Data.HoldLocation);
		}

		if (!LeaderPawn)
		{
			Data.TimeLookedAt = 0.f;
			Data.AimLostTime = 0.f;
			Data.TimeLeaderFocused = 0.f;
			Data.FocusLostTime = 0.f;
			Data.bFocusValid = false;
			Data.LeaderFocusActor = nullptr;
			Data.TimeLeaderStayingPut = 0.f;
			Data.bAnchorValid = false;
			Data.bLeaderSettled = false;
			Data.LeavingAreaTime = 0.f;
			return;
		}

		Data.DistanceToLeader = FVector::Dist(PawnLocation, LeaderPawn->GetActorLocation());
		Data.LeaderSpeed = LeaderPawn->GetVelocity().Size2D();
		Data.bLeaderIsMoving = Data.LeaderSpeed > Data.LeaderMovingSpeedThreshold;
		Data.bHasLineOfSightToLeader = Controller->LineOfSightTo(LeaderPawn);

		// Where the leader is looking from and towards. Eye contact below uses it within
		// EyeContactDistance; shared attention uses it at any range, since you can study something
		// with her well away from you.
		// GetViewRotation, not GetActorEyesViewPoint. For a pawn the latter hands back the actor
			// rotation, which carries no pitch, so a leader staring at the sky still counted as looking
			// straight at the companion. GetViewRotation is also the network-aware one: it uses the
			// control rotation when locally controlled and the replicated view pitch otherwise, so the
			// server reads a remote player's aim the same way that player sees it.
			// The camera, not the head. With an over-the-shoulder boom those are a long way apart, and
			// measuring the angle from the head while taking the direction from the camera bakes in a
			// constant error that no amount of careful aiming can remove: the player puts the crosshair
			// dead on the companion and the test still reads tens of degrees off.
			FVector LeaderEyes;
			FRotator LeaderViewRotation;
			if (AController* LeaderController = LeaderPawn->GetController())
			{
				LeaderController->GetPlayerViewPoint(LeaderEyes, LeaderViewRotation);
			}
			else
			{
				LeaderEyes = LeaderPawn->GetPawnViewLocation();
				LeaderViewRotation = LeaderPawn->GetViewRotation();
			}

			const FVector LeaderView = LeaderViewRotation.Vector();

		// Eye contact: the leader has to be close AND actually pointing their view at the companion.
		if (Data.DistanceToLeader <= Data.EyeContactDistance)
		{
			const FVector CompanionEyes = ViperCompanionStateTree::GetEyeApprox(*Pawn);
			const FVector ToCompanion = (CompanionEyes - LeaderEyes).GetSafeNormal();
			if (!ToCompanion.IsNearlyZero())
			{
				const float Dot = FMath::Clamp(FVector::DotProduct(LeaderView, ToCompanion), -1.f, 1.f);
				Data.LeaderAimErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

				// How much of the view the body actually fills at this distance. A fixed cone means
				// "near enough" grows with range; this keeps it the same gesture at any distance.
				// Measured from the camera, because that is where the angle above was measured from:
				// using the actor-to-actor distance here would size the target for the wrong viewpoint.
				const float ViewDistance = FVector::Dist(CompanionEyes, LeaderEyes);
				const float HalfWidth = FMath::Max(1.f, Data.EyeContactBodyRadius * Data.EyeContactTolerance);
				const float Subtended = FMath::RadiansToDegrees(FMath::Atan2(HalfWidth, FMath::Max(1.f, ViewDistance)));

				Data.EyeContactThresholdDegrees = FMath::Clamp(Subtended, FMath::Min(Data.MinEyeContactAngle, Data.EyeContactAngle), Data.EyeContactAngle);
				Data.bLeaderLooksAtMe = Data.LeaderAimErrorDegrees <= Data.EyeContactThresholdDegrees;
			}
		}

		// How long the look has been held, forgiving short glances away. bLeaderLooksAtMe above is
		// the instantaneous test; this is the one worth making decisions on, because an aim that
		// wobbles across the edge of a body passes and fails it several times a second.
		if (Data.bLeaderLooksAtMe)
		{
			Data.AimLostTime = 0.f;
			Data.TimeLookedAt += DeltaTime;
		}
		else
		{
			Data.AimLostTime += DeltaTime;
			if (Data.AimLostTime > Data.EyeContactBreakGrace)
			{
				Data.TimeLookedAt = 0.f;
			}
		}
		Data.bLeaderNoticesMe = Data.TimeLookedAt >= Data.EyeContactHoldTime;

		// Shared attention: what the leader is looking at, and for how long. Data only - whether she
		// looks too is the tree's call (ShareAttention).
		{
			FVector Candidate = FVector::ZeroVector;
			AActor* CandidateActor = nullptr;
			bool bHasCandidate = false;
			UWorld* World = Controller->GetWorld();

			// Looking at her is eye contact, not attention on a thing.
			if (!Data.bLeaderLooksAtMe && World)
			{
				// An interactable in the leader's sights wins over the raw ray: looking at the letter
				// means the letter even when the crosshair is on the table under it.
				// Both bodies ignored: with an over-the-shoulder camera the line from the leader's eyes
				// can clip their own character, and her body is not what is being looked at.
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ViperLeaderFocus), false, LeaderPawn);
				Params.AddIgnoredActor(Pawn);

				TArray<AActor*> Finds;
				ViperCompanionStateTree::GatherFinds(World, LeaderEyes, Data.FocusMaxDistance, Pawn, LeaderPawn, Finds);
				float BestAngle = TNumericLimits<float>::Max();
				for (AActor* Find : Finds)
				{
					const FVector ToFindRaw = Find->GetActorLocation() - LeaderEyes;
					const FVector ToFind = ToFindRaw.GetSafeNormal();
					const float Angle = FMath::RadiansToDegrees(
						FMath::Acos(FMath::Clamp(FVector::DotProduct(LeaderView, ToFind), -1.f, 1.f)));
					// Up close a thing fills more of the view than a fixed cone, so the margin grows.
					const float Margin = FMath::Max(Data.FocusSnapAngle, FMath::RadiansToDegrees(
						FMath::Atan2(Data.FocusTargetRadius, FMath::Max(1.f, ToFindRaw.Size()))));
					if (Angle > Margin || Angle >= BestAngle)
					{
						continue;
					}
					FHitResult Blocker;
					const bool bBlocked = World->LineTraceSingleByChannel(Blocker, LeaderEyes,
						Find->GetActorLocation(), ECC_Visibility, Params) && Blocker.GetActor() != Find;
					if (!bBlocked)
					{
						BestAngle = Angle;
						CandidateActor = Find;
						Candidate = Find->GetActorLocation();
						bHasCandidate = true;
					}
				}

				if (!CandidateActor)
				{
					FHitResult Hit;
					if (World->LineTraceSingleByChannel(Hit, LeaderEyes,
						LeaderEyes + LeaderView * Data.FocusMaxDistance, ECC_Visibility, Params))
					{
						Candidate = Hit.ImpactPoint;
						bHasCandidate = true;
					}
				}
			}

			// Same thing: the same interactable, or a plain point that has not wandered off.
			const bool bSameThing = bHasCandidate && Data.bFocusValid
				&& ((CandidateActor || Data.LeaderFocusActor)
					? CandidateActor == Data.LeaderFocusActor
					: FVector::Dist(Candidate, Data.LeaderFocusPoint) <= Data.FocusRadius);

			if (bSameThing)
			{
				Data.FocusLostTime = 0.f;
				Data.TimeLeaderFocused += DeltaTime;
			}
			else
			{
				// A glance away inside the grace keeps the old focus and its clock; past it, whatever is
				// in view now becomes the focus and the clock starts again. A thing beats a plain point at
				// once: with an aim on the edge of a clue the two alternate frame by frame, and every
				// point frame would otherwise restart the grace and keep the wall as the focus for good.
				Data.FocusLostTime += DeltaTime;
				if (!Data.bFocusValid || Data.FocusLostTime > Data.FocusBreakGrace
					|| (CandidateActor && !Data.LeaderFocusActor))
				{
					Data.bFocusValid = bHasCandidate;
					Data.LeaderFocusActor = CandidateActor;
					Data.LeaderFocusPoint = bHasCandidate ? Candidate : FVector::ZeroVector;
					Data.TimeLeaderFocused = 0.f;
					Data.FocusLostTime = 0.f;
				}
			}

			// Only a thing to examine counts. A plain point is still tracked (and logged), but staring
			// at a wall while the camera rests is not something to share - in the first test it froze
			// her for half a minute while the leader stood about.
			Data.bLeaderStudying = Data.bFocusValid && Data.LeaderFocusActor != nullptr
				&& !Data.bLeaderLooksAtMe && Data.TimeLeaderFocused >= Data.FocusHoldTime;
		}

		// Staying put, and where. The question is ground covered, not speed: the anchor is pinned
		// where the clock started and only re-pinned when the leader leaves the loitering circle,
		// so walking about one room keeps the clock running while setting off somewhere restarts it
		// within about a second.
		const FVector LeaderLocationNow = LeaderPawn->GetActorLocation();

		// Settled is a latch, and while it holds the anchor is frozen. Two reasons, and both were
		// found by watching it behave wrongly. If the anchor followed a settled leader the area
		// would travel with them, so leaving the area could never happen. And if the fact were the
		// raw clock instead of this latch, one step of the leader would end the companion's own
		// business - which is the opposite of her having any. Getting her started takes the leader
		// loitering; keeping her going only takes them staying inside the wider area.
		if (!Data.bLeaderSettled)
		{
			if (!Data.bAnchorValid
				|| FVector::Dist2D(LeaderLocationNow, Data.LeaderAnchor) > Data.LeaderLoiterRadius)
			{
				Data.LeaderAnchor = LeaderLocationNow;
				Data.bAnchorValid = true;
				Data.TimeLeaderStayingPut = 0.f;
			}
			else
			{
				Data.TimeLeaderStayingPut += DeltaTime;
			}

			// You cannot settle somewhere in the act of leaving it. Without this, a clock that had
			// nearly run out while the leader stood about finishes a step after they set off, so the
			// companion goes about her own business just as they walk away - and then stays at it for
			// the whole LeaderSettleRadius it takes them to leave. Measured: the latch engaging at
			// 300 cm/s only 62 cm from the anchor, on a leader walking off in a straight line.
			const FVector FromAnchor = LeaderLocationNow - Data.LeaderAnchor;
			const FVector LeaderVelocity = LeaderPawn->GetVelocity();
			const bool bLeavingAnchor = Data.bLeaderIsMoving
				&& (FromAnchor.X * LeaderVelocity.X + FromAnchor.Y * LeaderVelocity.Y) > 0.f;

			Data.bLeaderSettled = !bLeavingAnchor && Data.TimeLeaderStayingPut >= Data.LeaderSettleTime;
		}
		else
		{
			// Leaving, as opposed to pacing about: already out of the circle they loitered in and
			// still walking outwards. Waiting for the edge of the whole area instead left her at her
			// own business for seven metres of the leader walking off.
			const FVector FromAnchor = LeaderLocationNow - Data.LeaderAnchor;
			const FVector LeaderVelocity = LeaderPawn->GetVelocity();
			const float DistanceFromAnchor = FromAnchor.Size2D();
			const bool bWalkingOut = Data.bLeaderIsMoving
				&& DistanceFromAnchor > Data.LeaderLoiterRadius
				&& (FromAnchor.X * LeaderVelocity.X + FromAnchor.Y * LeaderVelocity.Y) > 0.f;
			Data.LeavingAreaTime = bWalkingOut ? Data.LeavingAreaTime + DeltaTime : 0.f;

			const bool bLeftArea = DistanceFromAnchor > Data.LeaderSettleRadius
				|| (Data.LeaveAreaConfirmTime > 0.f && Data.LeavingAreaTime >= Data.LeaveAreaConfirmTime);
			if (bLeftArea)
			{
				UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
					TEXT("facts | LEFT THE AREA %.0f from the anchor (%s)"), DistanceFromAnchor,
					DistanceFromAnchor > Data.LeaderSettleRadius ? TEXT("past the edge") : TEXT("walking out"));
				Data.bLeaderSettled = false;
				Data.LeaderAnchor = LeaderLocationNow;
				Data.TimeLeaderStayingPut = 0.f;
				Data.LeavingAreaTime = 0.f;
			}
			else
			{
				Data.TimeLeaderStayingPut += DeltaTime;
			}
		}

		Data.DistanceLeaderFromAnchor = FVector::Dist2D(LeaderLocationNow, Data.LeaderAnchor);

		// One row per sample with the whole pair on it, so a run can be replayed outside the engine:
		// the browser tuner reads exactly these columns. Everything here is already computed above.
		if (ViperCompanionStateTree::ShouldSampleCsv(Data.CsvCountdown, DeltaTime))
		{
			// With the first row, not at TreeStart: the tree starts when the controller possesses the
			// pawn, which on a scripted run happens before the console variables are applied, so a
			// header written there never appears and the rows arrive with no schema.
			if (!Data.bWroteCsvHeader)
			{
				Data.bWroteCsvHeader = true;
				UE_LOG(LogViperCompanion, Log,
					TEXT("csvhead,t,lx,ly,lyaw,lspd,cx,cy,cyaw,cspd,dist,aimerr,aimthr,eye,intent,mode,gx,gy,sdx,sdy"));
			}

			const FVector LeaderLocation = LeaderPawn->GetActorLocation();
			const FVector DebugGoal = Controller->GetDebugGoal();
			const FVector DebugSteer = Controller->GetDebugSteer();
			UE_LOG(LogViperCompanion, Log,
				TEXT("csv,%.3f,%.1f,%.1f,%.1f,%.0f,%.1f,%.1f,%.1f,%.0f,%.0f,%.1f,%.1f,%d,%.3f,%d,%.1f,%.1f,%.3f,%.3f"),
				Pawn->GetWorld() ? Pawn->GetWorld()->GetTimeSeconds() : 0.f,
				LeaderLocation.X, LeaderLocation.Y, LeaderPawn->GetActorRotation().Yaw, Data.LeaderSpeed,
				PawnLocation.X, PawnLocation.Y, Pawn->GetActorRotation().Yaw, Pawn->GetVelocity().Size2D(),
				Data.DistanceToLeader, Data.LeaderAimErrorDegrees, Data.EyeContactThresholdDegrees,
				Data.bLeaderLooksAtMe ? 1 : 0, Controller->GetDebugIntent(), Controller->GetDebugMode(),
				DebugGoal.X, DebugGoal.Y, DebugSteer.X, DebugSteer.Y);
		}

		if (ViperCompanionStateTree::ShouldTrace(Data.TraceCountdown, DeltaTime))
		{
			UE_LOG(LogViperCompanion, Log,
				TEXT("facts | stance=%s dist=%.0f leaderSpd=%.0f moving=%d | aim=%d err=%.1f thr=%.1f | los=%d speaking=%d sinceBreak=%.1f rnd=%.2f")
				TEXT(" | looked=%.1f notices=%d stayingPut=%.1f fromAnchor=%.0f settled=%d | focus=%.1f studying=%d on=%s"),
				Data.Stance == EViperCompanionStance::Follow ? TEXT("Follow") : TEXT("Hold"),
				Data.DistanceToLeader, Data.LeaderSpeed, Data.bLeaderIsMoving ? 1 : 0,
				Data.bLeaderLooksAtMe ? 1 : 0, Data.LeaderAimErrorDegrees, Data.EyeContactThresholdDegrees,
				Data.bHasLineOfSightToLeader ? 1 : 0, Data.bIsSpeaking ? 1 : 0,
				Data.TimeSinceLastIdleBreak, Data.RandomValue,
				Data.TimeLookedAt, Data.bLeaderNoticesMe ? 1 : 0,
				Data.TimeLeaderStayingPut, Data.DistanceLeaderFromAnchor, Data.bLeaderSettled ? 1 : 0,
				Data.TimeLeaderFocused, Data.bLeaderStudying ? 1 : 0,
				Data.LeaderFocusActor ? *Data.LeaderFocusActor->GetName() : (Data.bFocusValid ? TEXT("point") : TEXT("-")));
		}
	}
}

/***********************************************************************/
/*                     EVALUATOR: VIPER COMPANION                      */
/***********************************************************************/

void FViperCompanionEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	ViperCompanionStateTree::UpdateEvaluatorData(Context.GetInstanceData(*this), 0.f);
}

void FViperCompanionEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ViperCompanionStateTree::UpdateEvaluatorData(Context.GetInstanceData(*this), DeltaTime);
}

/***********************************************************************/
/*                      TASK: VIPER FOLLOW LEADER                      */
/***********************************************************************/

EStateTreeRunStatus FViperFollowLeaderTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.AIController || !Data.AIController->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.bMoving = false;
	Data.CurrentStopDistance = Data.StopFollowDistance;
	Data.TimeSinceSideSwap = 0.f;
	// Seeded at the interval so the first path goes out immediately.
	Data.TimeSinceRepath = Data.RepathInterval;
	Data.LastGoal = FVector::ZeroVector;
	Data.CachedMaxWalkSpeed = 0.f;
	Data.bPathing = false;
	Data.bWayIsClear = true;
	Data.TimeSinceObstacleCheck = Data.ObstacleCheckInterval;
	Data.SmoothedLeaderForward = FVector::ZeroVector;
	Data.SmoothedSteerDirection = FVector::ZeroVector;
	Data.CommandedSpeed = 0.f;
	Data.bReacting = false;
	Data.ReactionCountdown = 0.f;
	Data.StepAsideCountdown = 0.f;
	// Negative means "not measured yet": the first tick seeds them instead of blending in from zero.
	Data.SmoothedLeaderWidth = -1.f;
	Data.WidthCheckCountdown = 0.f;
	Data.BlendedFormationAngle = -1.f;
	Data.BlendedFormationDistance = -1.f;

	// Enter already standing on the side it is actually walking on, or the first swap sweeps in from
	// dead centre behind the leader for no reason.
	if (const AViperCompanionAIController* EnteringController = Cast<AViperCompanionAIController>(Data.AIController))
	{
		Data.SmoothedSide = static_cast<float>(EnteringController->GetFormationSide());
	}

	if (const UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Data.AIController))
	{
		Data.CachedMaxWalkSpeed = Movement->MaxWalkSpeed;

		// Carry the commitment, not the speed. Measuring it as a fraction of RunSpeed meant a companion
		// strolling beside the leader at 170 entered at 0.25, so the easing-off window collapsed to a
		// third of its tuned length and it stopped in two thirds of a second: the very jolt this exists
		// to remove. Either it was walking when the state changed or it was not.
		const float Speed = Movement->Velocity.Size2D();
		Data.Intent = Speed > 20.f ? 1.f : 0.f;
	}
	else
	{
		Data.Intent = 0.f;
	}

	if (Data.bComeToRest)
	{
		// Already moving and already slowing: the hysteresis must not wake it up again.
		Data.bMoving = true;
	}

	Data.TraceCountdown = 0.f;
	UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
		TEXT("follow | ENTER comeToRest=%d intent=%.2f"), Data.bComeToRest ? 1 : 0, Data.Intent);

	Data.bLoggedParams = false;

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FViperFollowLeaderTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AAIController* Controller = Data.AIController;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// No leader pawn for a moment (e.g. the leader is respawning): just wait.
	if (!Data.Leader)
	{
		if (ViperCompanionStateTree::ShouldTrace(Data.TraceCountdown, DeltaTime))
		{
			UE_LOG(LogViperCompanion, Warning, TEXT("follow | IDLE: Leader is null (is the task Leader input bound to the evaluator?)"));
		}
		return EStateTreeRunStatus::Running;
	}

	AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Controller);

	// Live tuning first, so everything below reads whatever was pushed in a moment ago rather than
	// what the asset was compiled with.
	ViperCompanionStateTree::ApplyTuning(Data, ViperCompanionStateTree::GetMovement(Controller));

	// The values actually in force, dumped on the first tick rather than on entry: on entry the
	// tuning above has not run yet, so the line reports the asset values and an A/B comparison
	// reads as though the two sides were configured identically. Which is exactly what it did.
	if (!Data.bLoggedParams && ViperCompanionStateTree::DebugLevel() > 0)
	{
		Data.bLoggedParams = true;
		UE_LOG(LogViperCompanion, Log,
			TEXT("follow | PARAMS form=%.0f/%.0f side=%.1fs/%.1fs hold=%d space=%.0f | band=%.0f/%.0f settle=%.0f/%.0f")
			TEXT(" | spd=%.0f/%.0f pace=%.0f rate=%.0f | ease=%.2f/%.2f/%.2f/%.2f/%.2f | face=%d/%.0f")
			TEXT(" | steerReach=%.0f headingFollow=%.2f"),
			Data.FormationDistance, Data.FormationAngle, Data.SideSwapInterval, Data.SideSwapTime,
			Data.bHoldBearingWhileLeaderStands ? 1 : 0, Data.PersonalSpaceRadius,
			Data.StartFollowDistance, Data.StopFollowDistance, Data.SettleStartDistance, Data.SettleStopDistance,
			Data.WalkSpeed, Data.RunSpeed, Data.PaceMatchTolerance, Data.SpeedChangeRate,
			Data.IntentRiseTime, Data.IntentFallTime, Data.SettleFallTime, Data.HeadingSmoothTime, Data.SteerSmoothTime,
			Data.bFaceLeaderWhenStopped ? 1 : 0, Data.FaceLeaderTurnRate,
			Data.MinSteerReach, Data.HeadingFollowWeight);
	}

	Data.TimeSinceSideSwap += DeltaTime;
	Data.TimeSinceRepath += DeltaTime;

	// Swap shoulders now and then: a person does not spend the whole game in your left footprint.
	if (CompanionController && Data.SideSwapInterval > 0.f && Data.TimeSinceSideSwap >= Data.SideSwapInterval)
	{
		Data.TimeSinceSideSwap = 0.f;
		CompanionController->FlipFormationSide();
	}

	// The side as a continuous value. Swapping it outright moves the slot four metres across the
	// leader back in one frame, and the companion answers that by setting off sideways; swept over
	// SideSwapTime the same swap reads as drifting across to your other shoulder as you walk.
	const float TargetSide = CompanionController ? static_cast<float>(CompanionController->GetFormationSide()) : 1.f;
	if (Data.SmoothedSide == 0.f || Data.SideSwapTime <= KINDA_SMALL_NUMBER)
	{
		Data.SmoothedSide = TargetSide;
	}
	else
	{
		// Span of 2 between the shoulders, so this rate spends SideSwapTime on the whole crossing.
		Data.SmoothedSide = FMath::FInterpConstantTo(Data.SmoothedSide, TargetSide, DeltaTime, 2.f / Data.SideSwapTime);
	}

	const float Side = Data.SmoothedSide;

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector LeaderLocation = Data.Leader->GetActorLocation();
	const FVector LeaderVelocity = Data.Leader->GetVelocity();
	const float LeaderSpeed = LeaderVelocity.Size2D();

	// While the leader moves, tuck in behind their heading; while they stand, behind their facing.
	FVector LeaderForward = LeaderSpeed > KINDA_SMALL_NUMBER
		? LeaderVelocity.GetSafeNormal2D()
		: Data.Leader->GetActorForwardVector().GetSafeNormal2D();
	if (LeaderForward.IsNearlyZero())
	{
		LeaderForward = FVector::ForwardVector;
	}

	// The slot sits a couple of metres out and swings with this heading, so reading it raw turns every
	// twitch of the leader into half a metre of goal travel: the companion ends up chasing a shiver.
	if (Data.SmoothedLeaderForward.IsNearlyZero())
	{
		Data.SmoothedLeaderForward = LeaderForward;
	}
	else if (Data.HeadingSmoothTime > KINDA_SMALL_NUMBER)
	{
		Data.SmoothedLeaderForward = FMath::VInterpTo(Data.SmoothedLeaderForward, LeaderForward, DeltaTime, 1.f / Data.HeadingSmoothTime).GetSafeNormal2D();
		if (Data.SmoothedLeaderForward.IsNearlyZero())
		{
			Data.SmoothedLeaderForward = LeaderForward;
		}
	}
	else
	{
		Data.SmoothedLeaderForward = LeaderForward;
	}

	LeaderForward = Data.SmoothedLeaderForward;

	// How much room there is across the leader's heading. A few times a second is plenty - the
	// answer changes as fast as walls do - and it is smoothed so a doorway flicking past for one
	// sample does not yank the slot behind them and straight back out.
	Data.WidthCheckCountdown -= DeltaTime;
	if (Data.WidthCheckCountdown <= 0.f)
	{
		Data.WidthCheckCountdown = 0.25f;
		const FVector Across = FVector::CrossProduct(FVector::UpVector, LeaderForward);
		const float MaxReach = FMath::Max(Data.SideBySideMinWidth, Data.CorridorWidth);
		const float Width = ViperCompanionStateTree::FreeWidthAcross(Pawn->GetWorld(), LeaderLocation, Across, MaxReach)
			+ 2.f * Data.Leader->GetSimpleCollisionRadius();
		Data.SmoothedLeaderWidth = Data.SmoothedLeaderWidth < 0.f
			? Width
			: FMath::Lerp(Data.SmoothedLeaderWidth, Width, 0.5f);
	}

	// The shape of the formation follows the space. Strolling in the open, people walk side by side;
	// in a corridor they fall in behind, a little closer - which is also personal distance shrinking
	// where there is less room, for free. Anything in between keeps the ordinary slot.
	float WantAngle = Data.FormationAngle;
	float WantDistance = Data.FormationDistance;
	if (Data.CorridorWidth > 0.f && Data.SmoothedLeaderWidth >= 0.f && Data.SmoothedLeaderWidth < Data.CorridorWidth)
	{
		WantAngle = 0.f;
		WantDistance = Data.FormationDistance * 0.85f;
	}
	else if (Data.SmoothedLeaderWidth > Data.SideBySideMinWidth && LeaderSpeed > Data.KeepWalkingLeaderSpeed
		&& LeaderSpeed < Data.SideBySideMaxLeaderSpeed)
	{
		WantAngle = Data.SideBySideAngle;
		WantDistance = Data.SideBySideDistance;
	}

	if (Data.BlendedFormationAngle < 0.f || Data.FormationBlendTime <= KINDA_SMALL_NUMBER)
	{
		Data.BlendedFormationAngle = WantAngle;
		Data.BlendedFormationDistance = WantDistance;
	}
	else
	{
		// Exponential, about 95 per cent of the way there after FormationBlendTime.
		const float BlendSpeed = 3.f / Data.FormationBlendTime;
		Data.BlendedFormationAngle = FMath::FInterpTo(Data.BlendedFormationAngle, WantAngle, DeltaTime, BlendSpeed);
		Data.BlendedFormationDistance = FMath::FInterpTo(Data.BlendedFormationDistance, WantDistance, DeltaTime, BlendSpeed);
	}

	// The slot: off one shoulder behind the leader, pushed forward by where they are heading,
	// so the companion cuts the corner instead of tracing the path the leader already walked.
	const FVector SlotDirection = FRotator(0.f, Side * Data.BlendedFormationAngle, 0.f).RotateVector(-LeaderForward);
	FVector Goal = LeaderLocation + SlotDirection * Data.BlendedFormationDistance + LeaderVelocity * Data.LeadTime;

	// Standing somewhere, rather than holding a slot that hangs off the leader. Once they have been
	// in one place for SettlePlantDelay she chooses a spot - a bearing off their back, rolled afresh
	// so she is not forever directly behind them - and keeps it until they leave, walk into it, or
	// drift far enough across the area that standing there is no longer standing with them. The
	// point is that the goal stops moving with them: their own pottering about then no longer drags
	// her along a step at a time, which is what read as being shadowed. Measured in the browser lab
	// over fourteen seconds of pottering: 169-1165 cm walked with this, 1288-1667 cm without.
	// The clock and the spot are the controller's, so that neither lasts only as long as the
	// current state selection. Without a companion controller there is nowhere to keep them, and
	// she simply holds the ordinary slot.
	AViperCompanionAIController* SpotKeeper = Cast<AViperCompanionAIController>(Controller);
	Data.bHasPlantedSpot = false;

	if (SpotKeeper)
	{
		SpotKeeper->UpdateStayingPut(LeaderLocation, Data.SettleLoiterRadius, DeltaTime);

		const bool bLeaderInOnePlace = Data.SettleLoiterRadius > 0.f && !Data.bComeToRest
			&& SpotKeeper->GetLeaderStayingPutTime() >= Data.SettlePlantDelay;

		if (!bLeaderInOnePlace)
		{
			SpotKeeper->ClearPlantedSpot();
		}
		else
		{
			const bool bHad = SpotKeeper->HasPlantedSpot();
			const float SpotToLeader = bHad
				? FVector::Dist2D(SpotKeeper->GetPlantedSpot(), LeaderLocation) : 0.f;
			const bool bSpotTaken = bHad && Data.PersonalSpaceRadius > 0.f
				&& SpotToLeader < Data.PersonalSpaceRadius;
			const bool bSpotLeftBehind = bHad
				&& SpotToLeader > Data.StartFollowDistance + Data.FormationDistance;

			if (bSpotTaken)
			{
				// The leader has walked into where she was standing. Their capsules have already
				// pushed her clear, so stand where that left her instead of rolling a fresh
				// bearing: re-rolling on every bump reads as dancing round them, and bumps are
				// common once someone is standing still in a doorway you want to walk through.
				FVector Out = (PawnLocation - LeaderLocation).GetSafeNormal2D();
				if (Out.IsNearlyZero())
				{
					Out = -LeaderForward;
				}

				FVector Stepped = LeaderLocation
					+ Out * FMath::Max(Data.PersonalSpaceRadius + 50.f,
						FVector::Dist2D(PawnLocation, LeaderLocation));
				FVector Projected;
				if (ViperCompanionStateTree::ProjectToNavigation(Pawn->GetWorld(), Stepped, Projected))
				{
					Stepped = Projected;
				}
				SpotKeeper->SetPlantedSpot(Stepped);
				SpotKeeper->ClearStandLook();

				UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
					TEXT("follow | SHOVED aside, standing %.0f from them"),
					FVector::Dist2D(Stepped, LeaderLocation));
			}
			else if (!bHad || bSpotLeftBehind)
			{
				FVector Chosen;
				if (ViperCompanionStateTree::PickStandingSpot(Pawn->GetWorld(), LeaderLocation, LeaderForward,
						Data.SettleBearingSpread, Data.PersonalSpaceRadius, Data.FormationDistance,
						Data.LeaderPathConeAngle, Data.LeaderPathConeLength, Data.MinStandWidth,
						Pawn->GetSimpleCollisionRadius(), Chosen))
				{
					SpotKeeper->SetPlantedSpot(Chosen);
					SpotKeeper->ClearStandLook();

					UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
						TEXT("follow | PLANT at %.0f from them, %.0f off dead behind (%s)"),
						FVector::Dist2D(Chosen, LeaderLocation),
						FMath::Abs(FMath::FindDeltaAngleDegrees(
							(-LeaderForward).Rotation().Yaw, (Chosen - LeaderLocation).Rotation().Yaw)),
						bSpotLeftBehind ? TEXT("you drifted off") : TEXT("new"));
				}
				else
				{
					SpotKeeper->ClearPlantedSpot();
				}
			}

			Data.bHasPlantedSpot = SpotKeeper->HasPlantedSpot();
		}
	}

	if (Data.bComeToRest)
	{
		// Coming to rest is not a reason to take up formation. The slot sits behind the leader, so
		// walking to it from face to face means crossing to their far side, which looks exactly like
		// circling them. Hold the bearing it already stands on and correct the distance only.
		FVector Bearing = (PawnLocation - LeaderLocation).GetSafeNormal2D();
		if (Bearing.IsNearlyZero())
		{
			Bearing = -LeaderForward;
		}
		Goal = LeaderLocation + Bearing * Data.ConversationDistance;
	}
	else if (Data.bHasPlantedSpot && SpotKeeper)
	{
		Goal = SpotKeeper->GetPlantedSpot();
	}
	else if (Data.bHoldBearingWhileLeaderStands && LeaderSpeed <= Data.KeepWalkingLeaderSpeed)
	{
		// The leader is standing. Their facing still turns, and the slot hangs off that facing, so
		// recomputing it here is exactly what walks the companion round them in a circle. Hold the
		// bearing it already stands on and correct the distance only: it closes in or it backs off,
		// but it never circles, because the goal stays radial to the leader.
		FVector Bearing = (PawnLocation - LeaderLocation).GetSafeNormal2D();
		if (Bearing.IsNearlyZero())
		{
			Bearing = -LeaderForward;
		}
		Goal = LeaderLocation + Bearing * Data.FormationDistance;
	}

	// The leader is walking at her and she is standing in their way. A person steps aside, to the
	// side they are already on; the slot alone would have her back away in front of them, or cut
	// across their path to reach their far shoulder. The raw velocity, not the smoothed heading:
	// this is about where they are going this second. The aside goal is held for a moment after
	// she leaves the cone, or she would step out, see the slot again and step straight back in.
	bool bSteppingAside = false;
	if (Data.StepAsideDistance > 0.f && !Data.bComeToRest)
	{
		const bool bWasSteppingAside = Data.StepAsideCountdown > 0.f;
		Data.StepAsideCountdown = FMath::Max(0.f, Data.StepAsideCountdown - DeltaTime);

		const FVector LeaderHeading = LeaderVelocity.GetSafeNormal2D();
		if (LeaderSpeed > Data.KeepWalkingLeaderSpeed && !LeaderHeading.IsNearlyZero()
			&& ViperCompanionStateTree::InLeaderPathCone(LeaderLocation, LeaderHeading, PawnLocation,
				Data.LeaderPathConeAngle, Data.StepAsideDistance))
		{
			const FVector ToPawn = FVector(PawnLocation.X - LeaderLocation.X, PawnLocation.Y - LeaderLocation.Y, 0.f);
			const float Along = FVector::DotProduct(ToPawn, LeaderHeading);
			FVector Lateral = ToPawn - LeaderHeading * Along;
			const float LateralNow = Lateral.Size2D();
			// Dead centre in front of them: pick her formation side rather than freezing.
			Lateral = LateralNow > 1.f ? Lateral / LateralNow
				: FVector::CrossProduct(FVector::UpVector, LeaderHeading) * (Side >= 0.f ? 1.f : -1.f);

			// Straight sideways from where she stands, far enough to clear their personal space.
			const float Clearance = FMath::Max(Data.PersonalSpaceRadius, 100.f) + 30.f;
			FVector Aside = LeaderLocation + LeaderHeading * Along + Lateral * FMath::Max(LateralNow, Clearance);
			FVector Projected;
			if (ViperCompanionStateTree::ProjectToNavigation(Pawn->GetWorld(), Aside, Projected))
			{
				Aside = Projected;
			}

			if (Data.StepAsideCountdown <= 0.f)
			{
				UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
					TEXT("follow | STEP ASIDE %.0f to the %s, you were %.0f off"),
					FMath::Max(0.f, Clearance - LateralNow),
					FVector::DotProduct(Lateral, FVector::CrossProduct(FVector::UpVector, LeaderHeading)) > 0.f ? TEXT("right") : TEXT("left"),
					Along);
			}

			Data.StepAsideGoal = Aside;
			Data.StepAsideCountdown = 0.8f;
		}

		if (Data.StepAsideCountdown > 0.f)
		{
			bSteppingAside = true;
			Goal = Data.StepAsideGoal;
		}
		else if (bWasSteppingAside && Data.bHasPlantedSpot && SpotKeeper)
		{
			// A spot of her own that was in their way is no longer a good spot: she stands where she
			// stepped to. Handed over once the step is done, not every frame of it.
			SpotKeeper->SetPlantedSpot(Data.StepAsideGoal);
			SpotKeeper->ClearStandLook();
		}
	}

	// A goal snapped onto a distant patch of navmesh is one way a companion ends up sprinting half a
	// level away in a dead straight line: the projection moves the goal, the distance-based speed
	// match reads the new distance as ground to make up, and nothing else in the loop disagrees.
	// Steering does not need the goal to be on the navmesh - what is in the way is handled below -
	// so a projection that moves it further than the formation distance is not believed.
	FVector ProjectedGoal = Goal;
	if (ViperCompanionStateTree::ProjectToNavigation(Pawn->GetWorld(), Goal, ProjectedGoal)
		&& FVector::DistSquared2D(ProjectedGoal, Goal) <= FMath::Square(FMath::Max(100.f, Data.FormationDistance)))
	{
		Goal = ProjectedGoal;
	}

	const float DistanceToLeader = FVector::Dist(PawnLocation, LeaderLocation);
	const float DistanceToGoal = FVector::Dist(PawnLocation, Goal);

	// Last resort when it got left behind entirely, and only while the leader is not looking at it.
	if (Data.TeleportDistance > 0.f && DistanceToLeader > Data.TeleportDistance)
	{
		const bool bWouldBeSeen = Data.bOnlyTeleportWhenUnseen && Controller->LineOfSightTo(Data.Leader);
		if (!bWouldBeSeen)
		{
			ViperCompanionStateTree::TeleportNearLeader(*Controller, *Pawn, *Data.Leader, Data.FormationDistance);
			Data.bMoving = false;
			return EStateTreeRunStatus::Running;
		}
	}

	// Settling up to a leader who has stopped is a different judgement from keeping pace with one
	// who has not: no slot is running away, there is only the gap left over from the walk. The wide
	// band that kills the stutter while walking is wider than that leftover, so without its own
	// narrower band the companion parks where the walk happened to end it and stays there.
	const bool bLeaderStanding = LeaderSpeed <= Data.KeepWalkingLeaderSpeed;

	// Coming to rest is an easing-off and not a walk, so it deliberately never starts moving. That
	// is right when it is easing to a stop at a sensible distance and wrong when it is already on
	// top of you: it then freezes there, and a measured run sat at ninety centimetres for a solid
	// minute because the walk in to recruit it had left the two of us that close when the look
	// began. Standing that close is not attention, it is crowding, so being inside the personal
	// space is the one case where coming to rest may still take a step - backwards.
	const bool bTooCloseToRest = Data.bComeToRest && Data.PersonalSpaceRadius > 0.f
		&& DistanceToLeader < Data.PersonalSpaceRadius;

	const bool bSettling = (bLeaderStanding || Data.bHasPlantedSpot) && Data.SettleStartDistance > 0.f
		&& (!Data.bComeToRest || bTooCloseToRest);
	float StartBand = bSettling ? Data.SettleStartDistance : Data.StartFollowDistance;
	float StopBand = bSettling ? Data.SettleStopDistance : Data.StopFollowDistance;
	float JitterBand = bSettling ? FMath::Min(Data.StopDistanceJitter, StopBand * 0.5f) : Data.StopDistanceJitter;

	// A chosen place is not a slot. The wide band that keeps a follow from stuttering would stop her
	// wherever the walk happened to end, and the bearing she picked would never show on the ground.
	if (Data.bHasPlantedSpot)
	{
		StartBand = FMath::Min(StartBand, 90.f);
		StopBand = FMath::Min(StopBand, 55.f);
		JitterBand = 0.f;
	}

	// Stepping aside is a short, deliberate move: the wide band would swallow it whole.
	if (bSteppingAside)
	{
		StartBand = FMath::Min(StartBand, 40.f);
		StopBand = FMath::Min(StopBand, 30.f);
		JitterBand = 0.f;
	}

	const bool bWasMoving = Data.bMoving;
	bool bWantsToStart = false;

	// Hysteresis: the gap between starting and stopping, plus random slack on the stop, is what
	// removes the start-stop stutter of a plain distance check.
	if (!Data.bMoving && DistanceToGoal > StartBand)
	{
		bWantsToStart = true;
	}
	else if (Data.bMoving && DistanceToGoal <= Data.CurrentStopDistance)
	{
		// No StopMovement here: the hysteresis decides what it wants, the ramp below decides how fast
		// that want changes. Cutting the input in one frame is what reads as a machine switching off.
		Data.bMoving = false;
	}

	// Walking beside someone who is still walking: the slot moves with them, so letting the distance
	// check halt the companion only means halting, losing the slot, and chasing it again a second later.
	// Pace is the speed match's job below; this flag is only ever about standing still.
	// Not while she is standing on a spot of her own: that is the whole point of having one.
	if (!Data.bComeToRest && !Data.bHasPlantedSpot && LeaderSpeed > Data.KeepWalkingLeaderSpeed)
	{
		if (bWasMoving)
		{
			Data.bMoving = true;
		}
		else
		{
			bWantsToStart = true;     // the leader has set off and she has not: that is a start
		}
	}

	// Reaction time. A start is decided now and happens a moment later, so the leader setting off
	// is answered the way a person answers it rather than on the same frame. Only from standing:
	// if she still carries intent from the last move she is already underway, and nobody pauses to
	// "react" to something they are already doing. A start that stops being wanted while she waits
	// is simply dropped.
	if (bWantsToStart)
	{
		const bool bAlreadyUnderway = Data.Intent > 0.f;
		if (!Data.bReacting)
		{
			Data.bReacting = true;
			const float Mean = Data.ReactionTimeMean;
			Data.ReactionCountdown = (Mean <= 0.f || bAlreadyUnderway) ? 0.f
				: FMath::Clamp(ViperCompanionStateTree::RandNormal(Mean, Data.ReactionTimeSpread), Mean * 0.43f, Mean * 2.f);
		}

		Data.ReactionCountdown -= DeltaTime;
		if (Data.ReactionCountdown <= 0.f || bAlreadyUnderway)
		{
			Data.bReacting = false;
			Data.bMoving = true;
			Data.CurrentStopDistance = StopBand + FMath::FRand() * JitterBand;
			Data.TimeSinceRepath = Data.RepathInterval;
		}
	}
	else
	{
		Data.bReacting = false;
	}

	// A state that exists to come to rest never wants to move again, however far the slot drifts.
	const float TargetIntent = (Data.bMoving && (!Data.bComeToRest || bTooCloseToRest)) ? 1.f : 0.f;

	// Easing off takes two different lengths of time. Letting a walk run on while it turns its
	// attention to you is the long one; stopping once it has walked up to you is short and
	// deliberate. Using the long one for both costs a metre twenty of glide, which carries it
	// straight through the spot it was aiming for and parks it too close.
	const float FallTime = (bSettling && Data.SettleFallTime > 0.f) ? Data.SettleFallTime : Data.IntentFallTime;
	const float RampTime = TargetIntent > Data.Intent ? Data.IntentRiseTime : FallTime;
	Data.Intent = RampTime > KINDA_SMALL_NUMBER
		? FMath::FInterpConstantTo(Data.Intent, TargetIntent, DeltaTime, 1.f / RampTime)
		: TargetIntent;

	// Below this the input moves nothing a viewer can see, and holding it alive only fights friction.
	// Only ever while easing off, though: clearing a rising intent is a deadlock, because one step of
	// the ramp is smaller than this threshold at a high frame rate (0.011 at 200fps against 0.02), so
	// it gets zeroed every frame and never climbs out. The companion then simply never starts walking,
	// and only on fast machines - which is exactly the kind of bug that survives hand testing.
	if (TargetIntent <= 0.f && Data.Intent <= 0.02f)
	{
		Data.Intent = 0.f;
		if (Data.bPathing)
		{
			Data.bPathing = false;
			Controller->StopMovement();
		}

		Data.CommandedSpeed = 0.f;
		Data.SmoothedSteerDirection = FVector::ZeroVector;
		if (CompanionController)
		{
			// The goal goes in even while parked. Leaving it at zero made the trace report a goal
			// distance of fourteen metres for a companion standing still a metre away, which sent
			// one diagnosis down the wrong road entirely.
			CompanionController->NotifyFollowDebug(0.f, 0, Goal, FVector::ZeroVector);
		}

		// Standing there with its back to you is the single thing that reads as switched off between
		// orders. Gives way at once if anything else has taken the body (see TurnTowardsWhileIdle).
		//
		// But facing the leader at every halt is its own tell: she looked at him through walls and
		// never at anything else. So unless he is looking at her - bComeToRest, where looking back
		// is the whole point - she looks at the nearest find, or down a way she could still walk.
		const bool bLookAbout = Data.bLookAroundWhenStopped && !Data.bComeToRest && CompanionController;

		if (bLookAbout)
		{
			if (!CompanionController->HasStandLook())
			{
				FVector Target;
				if (ViperCompanionStateTree::ChooseStandLook(Pawn->GetWorld(), *Pawn, Data.Leader,
						Data.StandLookRange, 1400.f, Data.LookForwardBias, Target))
				{
					CompanionController->SetStandLook(Target);

					UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
						TEXT("follow | LOOKING %.0f away, %.0f off the leader's bearing"),
						FVector::Dist2D(Target, PawnLocation),
						FMath::Abs(FMath::FindDeltaAngleDegrees(
							(Target - PawnLocation).Rotation().Yaw,
							(LeaderLocation - PawnLocation).Rotation().Yaw)));
				}
			}

			if (CompanionController->HasStandLook())
			{
				ViperCompanionStateTree::TurnTowardsWhileIdle(*Pawn, CompanionController->GetStandLook(),
					Data.FaceLeaderTurnRate, DeltaTime);
			}
		}
		else if (Data.bFaceLeaderWhenStopped
			&& (Data.FaceLeaderDistance <= 0.f || DistanceToLeader <= Data.FaceLeaderDistance))
		{
			ViperCompanionStateTree::TurnTowardsWhileIdle(*Pawn, LeaderLocation, Data.FaceLeaderTurnRate, DeltaTime);
		}

		if (ViperCompanionStateTree::ShouldTrace(Data.TraceCountdown, DeltaTime))
		{
			UE_LOG(LogViperCompanion, Log,
				TEXT("follow | PARKED intent=0 moving=%d goalDist=%.0f start=%.0f stop=%.0f rest=%d reacting=%d(%.2fs)"),
				Data.bMoving ? 1 : 0, DistanceToGoal, Data.StartFollowDistance, Data.CurrentStopDistance,
				Data.bComeToRest ? 1 : 0, Data.bReacting ? 1 : 0, FMath::Max(0.f, Data.ReactionCountdown));
		}

		return EStateTreeRunStatus::Running;
	}

	// Speed matching: the distance left sets the floor, the leader own pace pulls it up.
	if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Controller))
	{
		const float DistanceSpan = FMath::Max(1.f, Data.SpeedMatchDistance - Data.CurrentStopDistance);
		const float DistanceAlpha = FMath::Clamp((DistanceToGoal - Data.CurrentStopDistance) / DistanceSpan, 0.f, 1.f);
		const float LeaderAlpha = FMath::Clamp(LeaderSpeed / FMath::Max(1.f, Data.RunSpeed), 0.f, 1.f) * Data.LeaderSpeedInfluence;
		float TargetSpeed = FMath::Lerp(Data.WalkSpeed, Data.RunSpeed, FMath::Max(DistanceAlpha, LeaderAlpha));

		// Walking together, not tracking a setpoint. Once it is roughly where it wants to be and the
		// leader is walking, the honest answer to "how fast" is "as fast as you", and the gap is left
		// to breathe. Solving for the distance every frame is what makes the pace hunt audibly.
		if (!Data.bHasPlantedSpot && LeaderSpeed > Data.KeepWalkingLeaderSpeed
			&& DistanceToGoal <= Data.PaceMatchTolerance)
		{
			TargetSpeed = FMath::Clamp(LeaderSpeed, Data.WalkSpeed * 0.5f, Data.RunSpeed);

			// Side by side needs the gap along their heading closed too. Matching their pace leaves
			// whatever was left over when the match began - measured in the lab, a slot level with
			// their shoulder and her two metres behind it for the whole stroll. So, only as far as
			// the formation has turned into walking beside, the pace leans by the along-track error
			// (1 cm/s per cm), capped so it reads as drifting up alongside rather than as a sprint.
			const float BesideSpan = Data.SideBySideAngle - Data.FormationAngle;
			const float BesideAlpha = BesideSpan > KINDA_SMALL_NUMBER
				? FMath::Clamp((Data.BlendedFormationAngle - Data.FormationAngle) / BesideSpan, 0.f, 1.f) : 0.f;
			if (BesideAlpha > 0.f)
			{
				const FVector ToGoal(Goal.X - PawnLocation.X, Goal.Y - PawnLocation.Y, 0.f);
				const float AlongError = FVector::DotProduct(ToGoal, LeaderForward);
				TargetSpeed = FMath::Clamp(TargetSpeed + BesideAlpha * FMath::Clamp(AlongError, -0.5f * LeaderSpeed, 0.6f * LeaderSpeed),
					Data.WalkSpeed * 0.5f, Data.RunSpeed);
			}
		}

		// Someone strolling is not someone to sprint after. While the leader strolls, catching up is a
		// quickened step, unless she has genuinely been left behind.
		if (Data.StrollCatchUpFactor > 0.f && LeaderSpeed > Data.KeepWalkingLeaderSpeed
			&& LeaderSpeed < Data.SideBySideMaxLeaderSpeed
			&& FVector::Dist2D(PawnLocation, LeaderLocation) < Data.StrollCatchUpMaxGap)
		{
			TargetSpeed = FMath::Min(TargetSpeed, Data.WalkSpeed * Data.StrollCatchUpFactor);
		}

		// Legs do not change pace in one frame. Without this the slot swinging is enough to flick the
		// answer between walk and run several times along what is meant to be one straight walk.
		if (Data.CommandedSpeed <= 0.f || Data.SpeedChangeRate <= 0.f)
		{
			Data.CommandedSpeed = TargetSpeed;
		}
		else
		{
			Data.CommandedSpeed = FMath::FInterpConstantTo(Data.CommandedSpeed, TargetSpeed, DeltaTime, Data.SpeedChangeRate);
		}

		Movement->MaxWalkSpeed = FMath::Max(1.f, Data.CommandedSpeed);
	}

	// Steering or pathing, decided on a timer: a navmesh raycast every frame is wasted work.
	Data.TimeSinceObstacleCheck += DeltaTime;
	if (Data.bPreferDirectSteering && Data.TimeSinceObstacleCheck >= Data.ObstacleCheckInterval)
	{
		Data.TimeSinceObstacleCheck = 0.f;
		FVector Reachable;
		Data.bWayIsClear = ViperCompanionStateTree::FurthestClearPoint(Pawn->GetWorld(), PawnLocation, Goal, Reachable);
		Data.ClearPoint = Reachable;
		Data.ClearReach = Data.bWayIsClear ? 0.f : FVector::Dist2D(PawnLocation, Reachable);
	}

	// A line that only clips the edge of the walkable strip is not a reason to ask for a path. Steer
	// at the furthest point it can reach; that point slides along the edge as it walks, which rounds
	// the obstacle without ever handing the body over to path following. Only when it cannot get a
	// stride and a half down the line is it genuinely boxed in and the navmesh worth the cost.
	const bool bSteerToReachable = Data.bPreferDirectSteering && !Data.bWayIsClear
		&& Data.ClearReach >= Data.MinSteerReach;

	if (Data.bPreferDirectSteering && (Data.bWayIsClear || bSteerToReachable))
	{
		// Open ground, which is most of following someone: drive the pawn directly. No path means no
		// corners to re-aim at and no request to restart, so acceleration and turns stay continuous.
		if (Data.bPathing)
		{
			Data.bPathing = false;
			Controller->StopMovement();
		}

		const FVector SteerTarget = bSteerToReachable ? Data.ClearPoint : Goal;
		FVector SteerDirection = (SteerTarget - PawnLocation).GetSafeNormal2D();

		// Personal space. Reaching the slot is never worth brushing past the leader, so while it is
		// inside that gap part of the steering is spent getting back out of it. The push fades with
		// distance, so it nudges when it is a little close and shoves when it is about to touch.
		const float LeaderGap = FVector::Dist2D(PawnLocation, LeaderLocation);
		if (Data.PersonalSpaceRadius > 0.f && LeaderGap < Data.PersonalSpaceRadius && !SteerDirection.IsNearlyZero())
		{
			const FVector Away = (PawnLocation - LeaderLocation).GetSafeNormal2D();
			if (!Away.IsNearlyZero())
			{
				// Weighted well above 1 so that deep inside the gap the push wins outright: clearing
				// the leader matters more for one stride than reaching the slot a moment sooner.
				const float Crowding = 1.f - (LeaderGap / Data.PersonalSpaceRadius);
				SteerDirection = (SteerDirection + Away * Crowding * 2.5f).GetSafeNormal2D();
			}
		}

		// Near the slot and walking together: take most of the heading from where the leader is
		// going rather than from where the slot happens to be. The slot is a point two metres off
		// their shoulder, and aiming a body at it means the aim swings hard as it drifts, which is
		// what a fifth of a walk spent turning above 200 degrees a second actually was. The lateral
		// error does not need correcting in one stride; it washes out over the next few.
		// Not while she is standing on a spot of her own: blending in the leader's heading with her
		// own goal underfoot is exactly how a standing companion walks off alongside them.
		// Nor while stepping aside: the leader's heading points at her then, and blending it in is
		// exactly the backing-away-in-front-of-them that stepping aside replaces.
		if (!Data.bHasPlantedSpot && !bSteppingAside && Data.HeadingFollowWeight > 0.f
			&& LeaderSpeed > Data.KeepWalkingLeaderSpeed
			&& Data.PaceMatchTolerance > 0.f && DistanceToGoal < Data.PaceMatchTolerance
			&& !LeaderForward.IsNearlyZero() && !SteerDirection.IsNearlyZero())
		{
			const float Nearness = 1.f - FMath::Clamp(DistanceToGoal / Data.PaceMatchTolerance, 0.f, 1.f);
			const FVector Blended = FMath::Lerp(SteerDirection, LeaderForward, Nearness * Data.HeadingFollowWeight);
			if (!Blended.IsNearlyZero())
			{
				SteerDirection = Blended.GetSafeNormal2D();
			}
		}

		// Last filter before the pawn. Everything that can move the goal inside one frame - the side
		// sweeping across, the personal-space push coming on, the slot crossing behind the leader -
		// arrives here as a direction that flips, and a body that changes heading in one frame is the
		// clearest tell there is. One place to catch all of them.
		// Tried and withdrawn here: a hard constraint that removed any part of the steering pointing
		// at the leader while inside the gap, instead of the weighted push above. It reads better on
		// paper - a sum can cancel, a constraint cannot - and measured worse in four runs: 233 and
		// 295 jammed samples against 1 and 6, because the direction it leaves is tangential, and a
		// companion already touching your capsule slides along it without getting anywhere. If this
		// is revisited, the push has to separate the two bodies, not steer around one of them.
		if (SteerDirection.IsNearlyZero())
		{
			// Nowhere to go this frame: standing on the target, or the two halves of the steering
			// cancelling - the pull to a slot that has swapped to the leader far side against the
			// push out of their personal space, which are equal and opposite at six tenths of the
			// personal-space radius. Stand still. Reusing the last heading instead is a companion
			// walking in a dead straight line until something interrupts it, and it ran fifty
			// metres across the level that way, out and back, before this was measured.
			Data.SmoothedSteerDirection = FVector::ZeroVector;
		}
		else
		{
			if (Data.SmoothedSteerDirection.IsNearlyZero() || Data.SteerSmoothTime <= KINDA_SMALL_NUMBER)
			{
				Data.SmoothedSteerDirection = SteerDirection;
			}
			else
			{
				const FVector Smoothed = FMath::VInterpTo(Data.SmoothedSteerDirection, SteerDirection,
					DeltaTime, 1.f / Data.SteerSmoothTime).GetSafeNormal2D();

				// A turn through nearly 180 degrees passes through the origin, where normalising
				// gives nothing back. Take the new direction rather than keeping the old one.
				Data.SmoothedSteerDirection = Smoothed.IsNearlyZero() ? SteerDirection : Smoothed;
			}

			SteerDirection = Data.SmoothedSteerDirection;
			Pawn->AddMovementInput(SteerDirection, Data.Intent);
			if (CompanionController)
			{
				CompanionController->ClearStandLook();
			}
		}

		if (CompanionController)
		{
			CompanionController->NotifyFollowDebug(Data.Intent, 1, SteerTarget, SteerDirection);
		}

		if (ViperCompanionStateTree::DebugLevel() >= 2 && !Data.bTracedSteering)
		{
			Data.bTracedSteering = true;
			UE_LOG(LogViperCompanion, Log, TEXT("follow | mode -> STEER (way is clear)"));
		}

		if (ViperCompanionStateTree::ShouldTrace(Data.TraceCountdown, DeltaTime))
		{
			const float Speed = Pawn->GetVelocity().Size2D();
			UE_LOG(LogViperCompanion, Log,
				TEXT("follow | mode=steer intent=%.2f spd=%.0f goalDist=%.0f gap=%.0f crowd=%d side=%s rest=%d")
				TEXT(" | slot=%.0fdeg/%.0f width=%.0f aside=%d"),
				Data.Intent, Speed, DistanceToGoal, LeaderGap,
				(Data.PersonalSpaceRadius > 0.f && LeaderGap < Data.PersonalSpaceRadius) ? 1 : 0,
				Side > 0.f ? TEXT("R") : TEXT("L"), Data.bComeToRest ? 1 : 0,
				Data.BlendedFormationAngle, Data.BlendedFormationDistance, Data.SmoothedLeaderWidth,
				bSteppingAside ? 1 : 0);
		}

		return EStateTreeRunStatus::Running;
	}

	// Something solid in between: fall back on the navmesh, still throttled so the path does not thrash.
	const bool bPathIdle = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;
	const bool bGoalDrifted = FVector::Dist(Goal, Data.LastGoal) > Data.RepathTolerance;
	if (!Data.bPathing || bPathIdle || (Data.TimeSinceRepath >= Data.RepathInterval && bGoalDrifted))
	{
		Data.TimeSinceRepath = 0.f;
		Data.LastGoal = Goal;
		Data.bPathing = true;
		Controller->MoveToLocation(Goal, Data.CurrentStopDistance * 0.5f, true, true, false, false);
	}

	if (CompanionController)
	{
		CompanionController->NotifyFollowDebug(Data.Intent, 2, Goal, (Goal - PawnLocation).GetSafeNormal2D());
	}

	if (ViperCompanionStateTree::DebugLevel() >= 2 && Data.bTracedSteering)
	{
		Data.bTracedSteering = false;
		UE_LOG(LogViperCompanion, Log, TEXT("follow | mode -> PATH (something in the way)"));
	}

	if (ViperCompanionStateTree::ShouldTrace(Data.TraceCountdown, DeltaTime))
	{
		const float Speed = Pawn->GetVelocity().Size2D();
		UE_LOG(LogViperCompanion, Log,
			TEXT("follow | mode=path intent=%.2f spd=%.0f goalDist=%.0f gap=%.0f side=%s rest=%d"),
			Data.Intent, Speed, DistanceToGoal, FVector::Dist2D(PawnLocation, LeaderLocation),
			Side > 0.f ? TEXT("R") : TEXT("L"), Data.bComeToRest ? 1 : 0);
	}

	return EStateTreeRunStatus::Running;
}

void FViperFollowLeaderTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.AIController)
	{
		return;
	}

	// Say in the trace that nobody is following any more, so a mode read while another task owns the
	// state is not the last one this task happened to leave behind.
	if (AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Data.AIController))
	{
		CompanionController->NotifyFollowDebug(0.f, -1);
	}

	// Only cancel a path when there is one to cancel. While steering directly there is no request to
	// abort: we simply stop feeding input, and the momentum it already carries runs out on its own.
	// That leftover momentum is the point - it is what lets the next state be entered mid-stride.
	if (Data.bPathing)
	{
		Data.AIController->StopMovement();
	}

	// Hand the movement component back the speed it had: this task owns MaxWalkSpeed while it runs.
	if (Data.CachedMaxWalkSpeed > 0.f)
	{
		if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Data.AIController))
		{
			Movement->MaxWalkSpeed = Data.CachedMaxWalkSpeed;
		}
	}
}

/***********************************************************************/
/*                        TASK: VIPER LOOK AT                          */
/***********************************************************************/

namespace ViperCompanionStateTree
{
	/** Where the gaze should land. False when the task has neither an actor nor a point. */
	static bool ResolveGazePoint(const FViperLookAtTaskInstanceData& Data, FVector& OutPoint)
	{
		if (Data.Target)
		{
			OutPoint = Data.Target->GetActorLocation();
			return true;
		}
		if (!Data.TargetLocation.IsNearlyZero())
		{
			OutPoint = Data.TargetLocation;
			return true;
		}
		return false;
	}

	/**
	 * Body turning: AI focus only writes the control rotation, and the companion ignores it
	 * (bUseControllerRotationYaw is off, bOrientRotationToMovement is on). So while bTurnBody is
	 * active the flags are flipped, and put back on exit.
	 */
	static void BeginBodyRotationOverride(FViperLookAtTaskInstanceData& Data)
	{
		if (Data.bBodyRotationOverridden)
		{
			return;
		}

		APawn* Pawn = Data.AIController ? Data.AIController->GetPawn() : nullptr;
		UCharacterMovementComponent* Movement = GetMovement(Data.AIController);
		if (!Pawn || !Movement)
		{
			return;
		}

		Data.bCachedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		Data.bCachedUseControllerRotationYaw = Pawn->bUseControllerRotationYaw;
		Data.bBodyRotationOverridden = true;

		Movement->bOrientRotationToMovement = false;
		Pawn->bUseControllerRotationYaw = true;
	}

	static void EndBodyRotationOverride(FViperLookAtTaskInstanceData& Data)
	{
		if (!Data.bBodyRotationOverridden)
		{
			return;
		}

		APawn* Pawn = Data.AIController ? Data.AIController->GetPawn() : nullptr;
		if (UCharacterMovementComponent* Movement = GetMovement(Data.AIController))
		{
			Movement->bOrientRotationToMovement = Data.bCachedOrientRotationToMovement;
		}
		if (Pawn)
		{
			Pawn->bUseControllerRotationYaw = Data.bCachedUseControllerRotationYaw;
		}

		Data.bBodyRotationOverridden = false;
	}

	/** One spell of eye contact: normal round MeanHoldTime, clamped to the min/max. */
	static float SampleHoldTime(const FViperLookAtTaskInstanceData& Data)
	{
		const float Low = FMath::Min(Data.MinHoldTime, Data.MaxHoldTime);
		const float High = FMath::Max(Data.MinHoldTime, Data.MaxHoldTime);
		return FMath::Clamp(RandNormal(Data.MeanHoldTime, Data.HoldTimeSpread), Low, High);
	}

	static void ApplyGaze(FViperLookAtTaskInstanceData& Data)
	{
		AViperCompanionCharacter* Companion = GetCompanionCharacter(Data.AIController);
		if (!Companion)
		{
			return;
		}

		FVector TargetPoint;
		const bool bHasTarget = ResolveGazePoint(Data, TargetPoint);
		const bool bTooFar = bHasTarget && Data.MaxLookAtDistance > 0.f
			&& FVector::Dist(Companion->GetActorLocation(), TargetPoint) > Data.MaxLookAtDistance;

		if (!bHasTarget || bTooFar)
		{
			Companion->ClearLookAt();
			if (Data.AIController)
			{
				Data.AIController->ClearFocus(EAIFocusPriority::Gameplay);
			}
			EndBodyRotationOverride(Data);
			return;
		}

		const FVector EyeApprox = GetEyeApprox(*Companion);

		if (Data.bGlancingAway && !Data.GlanceOffsetDirection.IsNearlyZero())
		{
			// Eyes drift off to the side, head stays in the neighbourhood, commitment drops a little.
			const float Distance = FMath::Max(100.f, FVector::Dist(EyeApprox, TargetPoint));
			Companion->SetLookAtLocation(EyeApprox + Data.GlanceOffsetDirection * Distance, Data.Weight * 0.75f);
		}
		else if (Data.Target)
		{
			// Actor target: resolved every frame on each machine, so the gaze tracks a moving player.
			Companion->SetLookAtActor(Data.Target, Data.Weight);
		}
		else
		{
			Companion->SetLookAtLocation(TargetPoint, Data.Weight);
		}

		if (Data.bTurnBody && Data.AIController)
		{
			BeginBodyRotationOverride(Data);
			if (Data.Target)
			{
				Data.AIController->SetFocus(Data.Target, EAIFocusPriority::Gameplay);
			}
			else
			{
				Data.AIController->SetFocalPoint(TargetPoint, EAIFocusPriority::Gameplay);
			}
		}
	}
}

EStateTreeRunStatus FViperLookAtTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!ViperCompanionStateTree::GetCompanionCharacter(Data.AIController))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.ElapsedTime = 0.f;
	Data.bGlancingAway = false;
	Data.GlanceOffsetDirection = FVector::ZeroVector;
	Data.bBodyRotationOverridden = false;
	// Randomised from the first moment, so two companions never blink in unison.
	Data.PhaseCountdown = ViperCompanionStateTree::SampleHoldTime(Data);

	ViperCompanionStateTree::ApplyGaze(Data);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FViperLookAtTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime += DeltaTime;

	// Hold the gaze, glance away, come back. Nobody stares without blinking.
	if (Data.bBreakEyeContact)
	{
		Data.PhaseCountdown -= DeltaTime;
		if (Data.PhaseCountdown <= 0.f)
		{
			Data.bGlancingAway = !Data.bGlancingAway;

			if (Data.bGlancingAway)
			{
				Data.PhaseCountdown = FMath::FRandRange(FMath::Min(Data.MinGlanceAwayTime, Data.MaxGlanceAwayTime), FMath::Max(Data.MinGlanceAwayTime, Data.MaxGlanceAwayTime));

				// Bank the direction once, so the glance is a single smooth drift and not jitter.
				AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Data.AIController);
				FVector TargetPoint;
				if (Companion && ViperCompanionStateTree::ResolveGazePoint(Data, TargetPoint))
				{
					// Small and a little low: a look just past the face, not a turn of the head to
					// something else. A wide level glance reads as being distracted by something.
					const float Angle = FMath::FRandRange(Data.GlanceAwayAngle * 0.4f, Data.GlanceAwayAngle) * (FMath::RandBool() ? 1.f : -1.f);
					const FVector ToTarget = (TargetPoint - ViperCompanionStateTree::GetEyeApprox(*Companion)).GetSafeNormal();
					FVector Offset = ToTarget.RotateAngleAxis(Angle, FVector::UpVector);
					const FVector Right = FVector::CrossProduct(FVector::UpVector, Offset).GetSafeNormal();
					if (!Right.IsNearlyZero() && Data.GlanceDownAngle > 0.f)
					{
						// Positive rotation about the right axis tips the forward vector down (left-handed Z-up).
						Offset = Offset.RotateAngleAxis(FMath::FRandRange(0.5f, 1.f) * Data.GlanceDownAngle, Right);
					}
					Data.GlanceOffsetDirection = Offset;
				}
			}
			else
			{
				Data.PhaseCountdown = ViperCompanionStateTree::SampleHoldTime(Data);
				Data.GlanceOffsetDirection = FVector::ZeroVector;
			}
		}
	}

	ViperCompanionStateTree::ApplyGaze(Data);

	if (Data.Duration > 0.f && Data.ElapsedTime >= Data.Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FViperLookAtTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Data.AIController))
	{
		Companion->ClearLookAt();
	}
	if (Data.AIController)
	{
		Data.AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	ViperCompanionStateTree::EndBodyRotationOverride(Data);
}

/***********************************************************************/
/*                         TASK: VIPER WANDER                          */
/***********************************************************************/

EStateTreeRunStatus FViperWanderTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AAIController* Controller = Data.AIController;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.ElapsedTime = 0.f;
	Data.Destination = FVector::ZeroVector;
	Data.CachedMaxWalkSpeed = 0.f;
	Data.bArrived = false;
	Data.DwellCountdown = 0.f;
	Data.bAtFind = false;
	Data.GazeLeadPhase = 0;
	Data.GazeLeadCountdown = 0.f;
	Data.StepAsideCountdown = 0.f;
	Data.StepAsideGoal = FVector::ZeroVector;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
	if (!NavSystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector Origin = Pawn->GetActorLocation();

	// The area she is free to move about in. Centred on LeashCentre when the tree supplies one -
	// bind it to the evaluator LeaderAnchor and it is the ground the leader stopped on - and on
	// her own feet otherwise, which is the old behaviour and drifts.
	const bool bHasCentre = !Data.LeashCentre.IsNearlyZero();
	const FVector AreaCentre = bHasCentre ? Data.LeashCentre
		: (Data.Leader ? Data.Leader->GetActorLocation() : Origin);
	const FVector PickOrigin = bHasCentre ? Data.LeashCentre : Origin;
	const bool bLeashed = Data.MaxLeashFromLeader > 0.f && (bHasCentre || Data.Leader != nullptr);

	AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Controller);
	const AActor* LeaderActor = Data.Leader;
	Data.LookTarget = FVector::ZeroVector;
	bool bFound = false;

	// Where a walking leader is about to be. A spot there is one she would only have to leave again.
	const FVector LeaderHeading = LeaderActor ? LeaderActor->GetVelocity().GetSafeNormal2D() : FVector::ZeroVector;
	const bool bAvoidLeaderPath = LeaderActor && Data.AvoidLeaderPathLength > 0.f && !LeaderHeading.IsNearlyZero()
		&& LeaderActor->GetVelocity().Size2D() > Data.StepAsideMinLeaderSpeed;
	auto InLeaderPath = [&](const FVector& Point)
	{
		return bAvoidLeaderPath && ViperCompanionStateTree::InLeaderPathCone(LeaderActor->GetActorLocation(),
			LeaderHeading, Point, Data.LeaderPathConeAngle, Data.AvoidLeaderPathLength);
	};

	// First: something in the world worth examining. This is the whole point of her going off on
	// her own - a detective walks over to what can be looked at. Bare ground is the fallback.
	if (Data.bPreferInteractables)
	{
		const float AreaRadius = Data.MaxLeashFromLeader > 0.f ? Data.MaxLeashFromLeader : Data.WanderRadius;
		const AActor* LastExamined = CompanionController ? CompanionController->GetLastExamined() : nullptr;

		TArray<AActor*> Finds;
		ViperCompanionStateTree::GatherFinds(Pawn->GetWorld(), AreaCentre, AreaRadius, Pawn, LeaderActor, Finds);

		for (AActor* Find : Finds)
		{
			// Not the same thing twice running, as long as there is anything else to look at.
			if (Find == LastExamined && Finds.Num() > 1)
			{
				continue;
			}

			const FVector FindLocation = Find->GetActorLocation();
			FVector Away = (Origin - FindLocation).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = FVector::ForwardVector;
			}

			FVector Candidate = FindLocation + Away * Data.StandOffFromFind;
			FNavLocation Projected;
			if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(150.f, 150.f, 250.f)))
			{
				continue;
			}
			Candidate = Projected.Location;

			if (LeaderActor && Data.MinDistanceFromLeader > 0.f
				&& FVector::Dist2D(LeaderActor->GetActorLocation(), Candidate) < Data.MinDistanceFromLeader)
			{
				continue;
			}

			if (InLeaderPath(Candidate))
			{
				continue;
			}

			Data.Destination = Candidate;
			Data.LookTarget = FindLocation;
			bFound = true;
			Data.bAtFind = true;
			if (CompanionController)
			{
				CompanionController->SetLastExamined(Find);
			}

			UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
				TEXT("wander | FIND %s, standing %.0f short of it"),
				*Find->GetName(), FVector::Dist2D(Candidate, FindLocation));
			break;
		}
	}

	// Nothing to examine. Of a dozen reachable spots, take the one with the longest clear run still
	// ahead of it and stand looking down that run: otherwise she picks whatever corner the navmesh
	// offered and spends the whole dwell staring at a wall.
	if (!bFound)
	{
		// Forgetting the last find means the next look at this area starts from the objects again,
		// so she alternates between examining things and casting about.
		if (CompanionController)
		{
			CompanionController->SetLastExamined(nullptr);
		}

		float BestReach = -1.f;
		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			FNavLocation Candidate;
			if (!NavSystem->GetRandomReachablePointInRadius(PickOrigin, Data.WanderRadius, Candidate))
			{
				continue;
			}

			if (FVector::Dist(Origin, Candidate.Location) < Data.MinWanderDistance)
			{
				continue;
			}

			if (bLeashed && FVector::Dist(AreaCentre, Candidate.Location) > Data.MaxLeashFromLeader)
			{
				continue;
			}

			if (LeaderActor && Data.MinDistanceFromLeader > 0.f
				&& FVector::Dist(LeaderActor->GetActorLocation(), Candidate.Location) < Data.MinDistanceFromLeader)
			{
				continue;
			}

			if (InLeaderPath(Candidate.Location))
			{
				continue;
			}

			// The heading she will arrive on, so the spot is one she can stand at looking onwards.
			const float ArriveYaw = (Candidate.Location - Origin).Rotation().Yaw;
			FVector OpenDirection;
			const float Reach = ViperCompanionStateTree::OpenestDirection(
				Pawn->GetWorld(), Candidate.Location, Data.OpenGroundReach,
				ArriveYaw, Data.LookForwardBias, OpenDirection);
			if (Reach > BestReach)
			{
				BestReach = Reach;
				Data.Destination = Candidate.Location;
				Data.LookTarget = Candidate.Location
					+ OpenDirection * FMath::Clamp(Reach, 300.f, 800.f);
				bFound = true;
			}
		}

		UE_CLOG(bFound && ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
			TEXT("wander | GROUND, %.0f of clear ground ahead of the spot"), BestReach);
	}

	if (!bFound)
	{
		return EStateTreeRunStatus::Failed;
	}

	// A stroll, not a sprint: wandering is curiosity, not urgency.
	if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Controller))
	{
		Data.CachedMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = Data.WanderSpeed;
	}

	if (Data.bLookAtDestination)
	{
		if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Controller))
		{
			Companion->SetLookAtLocation(Data.LookTarget + FVector(0.f, 0.f, Pawn->BaseEyeHeight), 0.7f);
		}
	}

	if (CompanionController)
	{
		CompanionController->NotifyIdleBreak();
	}

	UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
		TEXT("wander | ENTER target %.0f away, %.0f from the area centre (%s), dwell %.1f"),
		FVector::Dist(Origin, Data.Destination),
		FVector::Dist(AreaCentre, Data.Destination),
		bHasCentre ? TEXT("anchored") : TEXT("drifting"),
		Data.DwellTime);

	Controller->MoveToLocation(Data.Destination, Data.AcceptanceRadius, true, true, false, false);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FViperWanderTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AAIController* Controller = Data.AIController;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.ElapsedTime += DeltaTime;

	// Timeout, so a blocked path or a nav island never wedges the state.
	if (Data.ElapsedTime >= Data.MaxWanderTime)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// The leader walking at her while she is about her own business: she steps out of their way,
	// to the side she is already on, and looks at them while she does - the way you yield to
	// someone. The follow task has the same reflex; this state used to have none, so pacing about
	// the area walked the leader straight into her. Held a moment after she leaves the cone, or she
	// would step out, resume, and step straight back in. Walking or dwelling both pause meanwhile.
	if (Data.StepAsideDistance > 0.f && Data.Leader)
	{
		const bool bWasSteppingAside = Data.StepAsideCountdown > 0.f;
		Data.StepAsideCountdown = FMath::Max(0.f, Data.StepAsideCountdown - DeltaTime);

		const FVector PawnLocation = Pawn->GetActorLocation();
		const FVector LeaderLocation = Data.Leader->GetActorLocation();
		const FVector LeaderVelocity = Data.Leader->GetVelocity();
		const FVector LeaderHeading = LeaderVelocity.GetSafeNormal2D();
		AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Controller);

		if (LeaderVelocity.Size2D() > Data.StepAsideMinLeaderSpeed && !LeaderHeading.IsNearlyZero()
			&& ViperCompanionStateTree::InLeaderPathCone(LeaderLocation, LeaderHeading, PawnLocation,
				Data.LeaderPathConeAngle, Data.StepAsideDistance))
		{
			const FVector ToPawn = FVector(PawnLocation.X - LeaderLocation.X, PawnLocation.Y - LeaderLocation.Y, 0.f);
			const float Along = FVector::DotProduct(ToPawn, LeaderHeading);
			FVector Lateral = ToPawn - LeaderHeading * Along;
			const float LateralNow = Lateral.Size2D();
			// Dead centre in front of them: either side, rather than freezing.
			Lateral = LateralNow > 1.f ? Lateral / LateralNow
				: FVector::CrossProduct(FVector::UpVector, LeaderHeading) * (FMath::RandBool() ? 1.f : -1.f);

			FVector Aside = LeaderLocation + LeaderHeading * Along + Lateral * FMath::Max(LateralNow, Data.StepAsideClearance);
			FVector Projected;
			if (ViperCompanionStateTree::ProjectToNavigation(Pawn->GetWorld(), Aside, Projected))
			{
				Aside = Projected;
			}

			if (!bWasSteppingAside || FVector::Dist2D(Aside, Data.StepAsideGoal) > 60.f)
			{
				if (!bWasSteppingAside)
				{
					if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Controller))
					{
						Movement->MaxWalkSpeed = FMath::Max(Data.WanderSpeed, Data.StepAsideSpeed);
					}
					UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
						TEXT("wander | STEP ASIDE %.0f to the %s, you were %.0f off"),
						FMath::Max(0.f, Data.StepAsideClearance - LateralNow),
						FVector::DotProduct(Lateral, FVector::CrossProduct(FVector::UpVector, LeaderHeading)) > 0.f ? TEXT("right") : TEXT("left"),
						Along);
					if (Companion && Data.bLookAtDestination)
					{
						Companion->SetLookAtActor(Data.Leader, 0.9f);
					}
				}
				Controller->MoveToLocation(Aside, 30.f, true, true, false, false);
			}

			Data.StepAsideGoal = Aside;
			Data.StepAsideCountdown = 0.8f;
		}

		if (Data.StepAsideCountdown > 0.f)
		{
			return EStateTreeRunStatus::Running;
		}

		if (bWasSteppingAside)
		{
			// Out of the way: back to what she was doing, from where she now stands.
			if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Controller))
			{
				Movement->MaxWalkSpeed = Data.WanderSpeed;
			}
			if (Companion && Data.bLookAtDestination)
			{
				Companion->SetLookAtLocation(Data.LookTarget + FVector(0.f, 0.f, Pawn->BaseEyeHeight), 0.9f);
			}
			if (Data.bArrived)
			{
				Controller->StopMovement();
			}
			else
			{
				Controller->MoveToLocation(Data.Destination, Data.AcceptanceRadius, true, true, false, false);
				return EStateTreeRunStatus::Running;
			}
		}
	}

	// Standing at whatever drew it over there. Walking somewhere and turning straight back is an
	// errand; stopping to look is what makes it read as having gone to see something.
	if (Data.bArrived)
	{
		// Gaze-leading: find -> leader -> find. Looking from a thing to you and back is how one
		// person shows another something without a word; it is also what tells the player that
		// this is something worth coming over to. Head only: the body stays on the find.
		if (Data.GazeLeadPhase == 1 || Data.GazeLeadPhase == 2)
		{
			Data.GazeLeadCountdown -= DeltaTime;
			if (Data.GazeLeadCountdown <= 0.f)
			{
				AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Controller);
				if (Data.GazeLeadPhase == 1 && Data.Leader && Companion)
				{
					Data.GazeLeadPhase = 2;
					Data.GazeLeadCountdown = Data.GazeLeadLeaderLookTime * FMath::FRandRange(0.8f, 1.2f);
					Companion->SetLookAtActor(Data.Leader, 0.9f);

					UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
						TEXT("wander | GAZE LEAD: glancing at you for %.2fs"), Data.GazeLeadCountdown);
				}
				else
				{
					Data.GazeLeadPhase = 3;
					if (Companion)
					{
						Companion->SetLookAtLocation(Data.LookTarget + FVector(0.f, 0.f, Pawn->BaseEyeHeight), 0.9f);
					}
				}
			}
		}

		Data.DwellCountdown -= DeltaTime;
		return Data.DwellCountdown <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}

	const bool bWithinRadius = FVector::Dist2D(Pawn->GetActorLocation(), Data.Destination) <= Data.AcceptanceRadius;
	// Path finished (arrived, or gave up): either way the walking part is over.
	const bool bPathDone = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;

	if (bWithinRadius || bPathDone)
	{
		Data.bArrived = true;
		Data.DwellCountdown = Data.DwellTime;

		if (Data.DwellTime <= 0.f)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		if (Data.bLookAtDestination)
		{
			if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Controller))
			{
				Companion->SetLookAtLocation(Data.LookTarget + FVector(0.f, 0.f, Pawn->BaseEyeHeight), 0.9f);
			}

			// Something to show, and someone near enough to show it to who can see her do it.
			const bool bLeaderNear = Data.Leader
				&& FVector::Dist(Pawn->GetActorLocation(), Data.Leader->GetActorLocation()) <= Data.GazeLeadMaxDistance;
			if (Data.bAtFind && Data.bGazeLeadOnFinds && bLeaderNear && Controller->LineOfSightTo(Data.Leader))
			{
				Data.GazeLeadPhase = 1;
				Data.GazeLeadCountdown = Data.GazeLeadFindLookTime * FMath::FRandRange(0.8f, 1.2f);
			}
		}

		UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
			TEXT("wander | ARRIVED after %.1fs, standing here %.1fs looking %.0f away"),
			Data.ElapsedTime, Data.DwellTime, FVector::Dist2D(Pawn->GetActorLocation(), Data.LookTarget));

		// Eases to a stop on arrival instead of parking on the spot.
		ViperCompanionStateTree::BeginSoftStop(*Controller, 0.6f);
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Running;
}

void FViperWanderTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.AIController)
	{
		return;
	}

	Data.AIController->StopMovement();

	if (Data.CachedMaxWalkSpeed > 0.f)
	{
		if (UCharacterMovementComponent* Movement = ViperCompanionStateTree::GetMovement(Data.AIController))
		{
			Movement->MaxWalkSpeed = Data.CachedMaxWalkSpeed;
		}
	}

	if (Data.bLookAtDestination)
	{
		if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Data.AIController))
		{
			Companion->ClearLookAt();
		}
	}
}

/***********************************************************************/
/*                       TASK: VIPER IDLE BREAK                        */
/***********************************************************************/

EStateTreeRunStatus FViperIdleBreakTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AAIController* Controller = Data.AIController;
	if (!Controller || !Controller->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.Countdown = FMath::FRandRange(FMath::Min(Data.MinDuration, Data.MaxDuration), FMath::Max(Data.MinDuration, Data.MaxDuration));
	// First glance comes soon but not instantly, so it does not snap its head the moment it stops.
	Data.GlanceCountdown = FMath::FRandRange(0.2f, FMath::Max(0.3f, Data.MinGlanceInterval));

	// Rolls to a halt over StopTime rather than dropping the input dead, so entering this state while
	// walking reads as easing off mid-stride instead of hitting a wall.
	Data.CachedBrakingDeceleration = ViperCompanionStateTree::BeginSoftStop(*Controller, Data.StopTime);

	UE_CLOG(ViperCompanionStateTree::DebugLevel() > 0, LogViperCompanion, Log,
		TEXT("idlebreak | ENTER for %.1fs stopTime=%.1f lookAround=%d"),
		Data.Countdown, Data.StopTime, Data.bLookAround ? 1 : 0);

	if (Data.bRegisterAsIdleBreak)
	{
		if (AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Controller))
		{
			CompanionController->NotifyIdleBreak();
		}
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FViperIdleBreakTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	APawn* Pawn = Data.AIController ? Data.AIController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.Countdown -= DeltaTime;

	// Eyes keep working while the body does nothing: that is what reads as "being somewhere".
	if (Data.bLookAround)
	{
		Data.GlanceCountdown -= DeltaTime;
		if (Data.GlanceCountdown <= 0.f)
		{
			Data.GlanceCountdown = FMath::FRandRange(FMath::Min(Data.MinGlanceInterval, Data.MaxGlanceInterval), FMath::Max(Data.MinGlanceInterval, Data.MaxGlanceInterval));

			if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Data.AIController))
			{
				const float Yaw = FMath::FRandRange(-Data.GlanceArc, Data.GlanceArc);
				// A slight downward bias: people scan the ground and the middle distance, not the sky.
				const float Pitch = FMath::FRandRange(-18.f, 8.f);
				const FVector Direction = Pawn->GetActorRotation().RotateVector(FRotator(Pitch, Yaw, 0.f).Vector());

				const FVector EyeApprox = ViperCompanionStateTree::GetEyeApprox(*Pawn);
				Companion->SetLookAtLocation(EyeApprox + Direction * Data.GlanceRadius, FMath::FRandRange(0.6f, 1.f));
			}
		}
	}

	return Data.Countdown <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FViperIdleBreakTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.bLookAround)
	{
		if (AViperCompanionCharacter* Companion = ViperCompanionStateTree::GetCompanionCharacter(Data.AIController))
		{
			Companion->ClearLookAt();
		}
	}

	// Give the movement component its own braking back: this task borrowed it to size the stop.
	if (Data.AIController)
	{
		ViperCompanionStateTree::RestoreBraking(*Data.AIController, Data.CachedBrakingDeceleration);
	}
}

/***********************************************************************/
/*                          TASK: VIPER SPEAK                          */
/***********************************************************************/

EStateTreeRunStatus FViperSpeakTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;

	AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Data.AIController);
	if (!CompanionController)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Fails when there is no line or no leader to hear it: let the tree pick something else.
	if (!CompanionController->Speak(Data.VoiceLine, Data.Duration))
	{
		return EStateTreeRunStatus::Failed;
	}

	return Data.bWaitForEnd ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FViperSpeakTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.bWaitForEnd)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	Data.ElapsedTime += DeltaTime;
	return Data.ElapsedTime >= Data.Duration ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

/***********************************************************************/
/*                    TASK: VIPER ACTIVATE ABILITY                     */
/***********************************************************************/

EStateTreeRunStatus FViperActivateAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ElapsedTime = 0.f;

	UAbilitySystemComponent* ASC = ViperCompanionStateTree::GetPawnAbilitySystem(Data.AIController);
	if (!ASC || !Data.AbilityTag.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Data.AbilityTag)))
	{
		return EStateTreeRunStatus::Failed;
	}

	return Data.bWaitForAbilityEnd ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FViperActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.bWaitForAbilityEnd)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	Data.ElapsedTime += DeltaTime;
	if (Data.MaxWaitTime > 0.f && Data.ElapsedTime >= Data.MaxWaitTime)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	UAbilitySystemComponent* ASC = ViperCompanionStateTree::GetPawnAbilitySystem(Data.AIController);
	if (!ASC)
	{
		return EStateTreeRunStatus::Failed;
	}

	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(Data.AbilityTag), MatchingSpecs, false);
	for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
	{
		if (Spec && Spec->IsActive())
		{
			return EStateTreeRunStatus::Running;
		}
	}

	return EStateTreeRunStatus::Succeeded;
}
