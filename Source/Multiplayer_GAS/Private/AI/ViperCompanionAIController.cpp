// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/ViperCompanionAIController.h"

#include "AI/ViperCompanionCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "Player/ViperPlayerController.h"
#include "Sound/SoundBase.h"
#include "StateTree.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Companion_Order, "Companion.Order", "Orders a player can give to their companion");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Companion_Order_Follow, "Companion.Order.Follow", "Come back and follow me");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Companion_Order_MoveTo, "Companion.Order.MoveTo", "Go to where I'm aiming and wait there");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Companion_Event_Reevaluate, "Companion.Event.Reevaluate", "StateTree event: leader, stance or order changed");

AViperCompanionAIController::AViperCompanionAIController()
{
	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	// Started in OnPossess: the StateTree "Actor" context is the pawn, which doesn't exist at BeginPlay.
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	bWantsPlayerState = false;
}

void AViperCompanionAIController::OnPossess(APawn* InPawn)
{
	// Super calls InPawn->PossessedBy, where the companion initializes its ability system.
	Super::OnPossess(InPawn);

	if (const AViperCompanionCharacter* Companion = Cast<AViperCompanionCharacter>(InPawn))
	{
		if (UStateTree* CompanionStateTree = Companion->GetCompanionStateTree())
		{
			StateTreeAIComponent->SetStateTree(CompanionStateTree);
		}
	}

	// Two companions spawned side by side shouldn't mirror each other: give each a random starting side.
	FormationSide = FMath::RandBool() ? 1 : -1;

	StateTreeAIComponent->StartLogic();
}

void AViperCompanionAIController::OnUnPossess()
{
	StateTreeAIComponent->StopLogic(TEXT("Unpossessed"));

	Super::OnUnPossess();
}

void AViperCompanionAIController::SetLeader(AController* NewLeader)
{
	if (Leader.Get() == NewLeader)
	{
		return;
	}

	Leader = NewLeader;
	Stance = EViperCompanionStance::Follow;
	HoldLocation = FVector::ZeroVector;

	// Start the idle clock on being taken on. It is seeded far in the past so a companion that has been
	// standing around may break early, but for one just recruited that reads as wandering off the moment
	// you hire it: every idle threshold is already satisfied on the very first tick.
	if (NewLeader)
	{
		LastIdleBreakTime = GetWorldTime();
	}

	if (!NewLeader)
	{
		StopMovement();
	}

	SendCompanionEvent(TAG_Companion_Event_Reevaluate);
}

void AViperCompanionAIController::IssueOrder(FGameplayTag OrderTag, const FVector& Location, AActor* TargetActor)
{
	if (!Leader.IsValid())
	{
		return;
	}

	if (OrderTag.MatchesTagExact(TAG_Companion_Order_Follow))
	{
		Stance = EViperCompanionStance::Follow;
	}
	else if (OrderTag.MatchesTagExact(TAG_Companion_Order_MoveTo))
	{
		Stance = EViperCompanionStance::HoldPosition;
		HoldLocation = Location;
	}
	else
	{
		return;
	}

	SendCompanionEvent(TAG_Companion_Event_Reevaluate);
}

APawn* AViperCompanionAIController::GetLeaderPawn() const
{
	const AController* LeaderController = Leader.Get();
	return LeaderController ? LeaderController->GetPawn() : nullptr;
}

float AViperCompanionAIController::GetTimeSinceLastVoice() const
{
	return static_cast<float>(GetWorldTime() - LastVoiceTime);
}

bool AViperCompanionAIController::Speak(TSoftObjectPtr<USoundBase> VoiceLine, float Duration)
{
	AViperPlayerController* LeaderPlayerController = Cast<AViperPlayerController>(Leader.Get());
	if (VoiceLine.IsNull() || !LeaderPlayerController)
	{
		return false;
	}

	const double Now = GetWorldTime();
	LastVoiceTime = Now;
	SpeakingEndTime = Now + FMath::Max(0.f, Duration);

	// The companion's mesh can play a talk animation on every machine while this lasts.
	if (AViperCompanionCharacter* Companion = Cast<AViperCompanionCharacter>(GetPawn()))
	{
		Companion->SetSpeaking(true, Duration);
	}

	LeaderPlayerController->ClientPlayCompanionVoice(VoiceLine);
	return true;
}

bool AViperCompanionAIController::IsSpeaking() const
{
	return GetWorldTime() < SpeakingEndTime;
}

void AViperCompanionAIController::NotifyIdleBreak()
{
	LastIdleBreakTime = GetWorldTime();
}

float AViperCompanionAIController::GetTimeSinceLastIdleBreak() const
{
	return static_cast<float>(GetWorldTime() - LastIdleBreakTime);
}

void AViperCompanionAIController::UpdateStayingPut(const FVector& LeaderLocation, float LoiterRadius, float DeltaTime)
{
	if (LoiterRadius <= 0.f)
	{
		bStandAnchorValid = false;
		LeaderStayingPutTime = 0.f;
		bHasPlantedSpot = false;
		return;
	}

	// The anchor is pinned where the clock started, not dragged along: the question is ground
	// covered, not speed, so walking about one room keeps the clock running while setting off
	// somewhere restarts it. Leaving the circle also gives up the spot - she is not standing in
	// one place to keep company with someone who has gone.
	if (!bStandAnchorValid || FVector::Dist2D(LeaderLocation, LeaderStandAnchor) > LoiterRadius)
	{
		LeaderStandAnchor = LeaderLocation;
		bStandAnchorValid = true;
		LeaderStayingPutTime = 0.f;
		bHasPlantedSpot = false;
	}
	else
	{
		LeaderStayingPutTime += DeltaTime;
	}
}

void AViperCompanionAIController::FlipFormationSide()
{
	FormationSide = -FormationSide;
}

void AViperCompanionAIController::SendCompanionEvent(FGameplayTag EventTag)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	// Sent on the next tick: this can be called while the StateTree is ticking (e.g. from a task).
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, EventTag]()
	{
		if (StateTreeAIComponent && StateTreeAIComponent->IsRunning())
		{
			StateTreeAIComponent->SendStateTreeEvent(EventTag);
		}
	}));
}

double AViperCompanionAIController::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
