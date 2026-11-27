// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/ViperTestPilotController.h"

#include "AI/ViperCompanionStateTreeNodes.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Info.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "NavigationSystem.h"
// SimpleMoveTo gives a player controller a path following component; this is where its status
// enum lives, and the status is how arrival is noticed.
#include "Navigation/PathFollowingComponent.h"
#include "Player/ViperPlayerCharacter.h"

AViperTestPilotController::AViperTestPilotController()
{
	PrimaryActorTick.bCanEverTick = true;

	// Nobody is watching through this one: no camera to manage, no input to read. It exists to
	// own a pawn and to count as a player while it does.
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = false;
}

AViperTestPilotController* AViperTestPilotController::Launch(UWorld* World, const FVector& Where,
	TSubclassOf<APawn> PawnClass)
{
	if (!World)
	{
		return nullptr;
	}

	// The game's own player pawn by default: a pilot driving something else would be testing
	// something else.
	if (!PawnClass)
	{
		if (const AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			PawnClass = GameMode->DefaultPawnClass;
		}
	}
	if (!PawnClass)
	{
		PawnClass = AViperPlayerCharacter::StaticClass();
	}

	FVector SpawnAt = Where;
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		// A rough spot is fine: put it on the navmesh, or it spawns inside a wall and the first
		// order fails for a reason that has nothing to do with what is being tested.
		FNavLocation Projected;
		if (NavSystem->ProjectPointToNavigation(Where, Projected, FVector(500.f, 500.f, 500.f)))
		{
			SpawnAt = Projected.Location + FVector(0.f, 0.f, 90.f);
		}
	}

	// It has to be a *local* player controller, not merely a player controller.
	//
	// Measured the hard way: a PlayerController spawned by hand has no ULocalPlayer, so
	// IsLocalController() is false, so the pawn is not locally controlled, so
	// UCharacterMovementComponent never moves it. Path following reported Moving for five
	// seconds while the pawn stood still with zero velocity. An AIController counts as local,
	// which is why the first version walked and this one would not.
	//
	// So the pilot is created as a second local player, the way splitscreen does it. That also
	// makes it a player for every other purpose, which is the whole reason for not using an AI.
	UGameInstance* GameInstance = World->GetGameInstance();
	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameInstance || !GameMode)
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | no game instance or game mode to add a player to"));
		return nullptr;
	}

	// Two local players would otherwise split the screen, which nobody asked for in a test.
	if (UGameViewportClient* Viewport = World->GetGameViewport())
	{
		Viewport->SetForceDisableSplitscreen(true);
	}

	const TSubclassOf<APlayerController> RealControllerClass = GameMode->PlayerControllerClass;
	GameMode->PlayerControllerClass = AViperTestPilotController::StaticClass();

	FString Error;
	ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(-1, Error, true);

	GameMode->PlayerControllerClass = RealControllerClass;

	AViperTestPilotController* Pilot = LocalPlayer
		? Cast<AViperTestPilotController>(LocalPlayer->PlayerController) : nullptr;
	if (!Pilot)
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | could not add a local player: %s"),
			Error.IsEmpty() ? TEXT("the controller was not of the pilot class") : *Error);
		return nullptr;
	}

	// The game mode spawned and possessed a pawn of its own default class at a player start.
	// Move it to where it was asked for, which is usually beside whoever typed the command.
	if (APawn* ItsPawn = Pilot->GetPawn())
	{
		ItsPawn->TeleportTo(SpawnAt, ItsPawn->GetActorRotation());
	}
	else
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | the game mode gave it no pawn"));
	}

	return Pilot;
}

TArray<AViperTestPilotController*> AViperTestPilotController::All(UWorld* World)
{
	TArray<AViperTestPilotController*> Found;
	if (!World)
	{
		return Found;
	}

	for (TActorIterator<AViperTestPilotController> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Found.Add(*It);
		}
	}
	return Found;
}

void AViperTestPilotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	OrderStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastOrder = TEXT("idle");

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | FLYING %s at %s"),
		InPawn ? *InPawn->GetName() : TEXT("nothing"),
		InPawn ? *InPawn->GetActorLocation().ToCompactString() : TEXT("-"));
}

void AViperTestPilotController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Held gaze: keeps the target in view while it moves, which a one-off turn cannot.
	if (LookTarget.IsValid())
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now < LookUntil)
		{
			AimAt(*LookTarget);
		}
		else
		{
			UE_LOG(LogViperCompanion, Log, TEXT("pilot | stopped looking at %s"), *LookTarget->GetName());
			LookTarget.Reset();
		}
	}

	if (!bMoving)
	{
		return;
	}

	// An AIController would get OnMoveCompleted. A player controller has to watch the path
	// following component SimpleMoveTo gave it, which is no worse: the status is the same fact.
	const UPathFollowingComponent* Following = FindComponentByClass<UPathFollowingComponent>();
	if (Following && Following->GetStatus() != EPathFollowingStatus::Idle)
	{
		// Once a second while it is supposed to be walking: "the component says Moving" and "the
		// pawn is moving" are different claims, and the first one was true while the second was
		// not for a whole run.
		static double LastSaid = 0.0;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now - LastSaid > 1.0)
		{
			LastSaid = Now;
			const APawn* MyPawn = GetPawn();
			const UPawnMovementComponent* Movement = MyPawn ? MyPawn->GetMovementComponent() : nullptr;
			UE_LOG(LogViperCompanion, Log,
				TEXT("pilot | walking: speed %.0f, movement comp %s, accel %s"),
				MyPawn ? MyPawn->GetVelocity().Size2D() : -1.f,
				Movement ? *Movement->GetName() : TEXT("NONE"),
				Movement ? *Movement->Velocity.ToCompactString() : TEXT("-"));
		}
		return;
	}

	bMoving = false;
	const float Elapsed = GetWorld()
		? static_cast<float>(GetWorld()->GetTimeSeconds() - OrderStartedAt) : 0.f;

	const AActor* Target = OrderTarget.Get();
	const float Remaining = (Target && GetPawn())
		? FVector::Dist2D(GetPawn()->GetActorLocation(), Target->GetActorLocation()) : 0.f;

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | ARRIVED after %.1fs, %.0f from the target"),
		Elapsed, Remaining);

	LastOrder = TEXT("idle");

	// Walking somewhere in order to use it: the interact can only happen now, once the walk is
	// over, because a console fires every command in the same frame.
	if (bInteractOnArrival)
	{
		bInteractOnArrival = false;

		// Facing it first: the interact trace goes out from where the pawn is looking.
		if (AActor* LookAt = OrderTarget.Get())
		{
			OrderLookAt(LookAt);
		}

		// And a moment later, not this frame. The character picks what it is looking at in its
		// own Tick, so interacting in the same frame as the turn asks it about the direction it
		// was facing while it walked. Measured: "nothing selected, 1 actor(s) in range" - she was
		// found and in reach, and the frame was simply one too early.
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this,
				[this]() { OrderInteract(); }), 0.2f, false);
		}
	}

	// And on to the next stop, after whatever happened here has had time to happen.
	if (!TourStops.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this,
				[this]() { AdvanceTour(); }), 0.8f, false);
		}
	}
}

bool AViperTestPilotController::OrderGoToLocation(const FVector& Target, float AcceptanceRadius)
{
	if (!GetPawn())
	{
		return false;
	}

	OrderTarget = nullptr;
	LastOrder = FString::Printf(TEXT("walk to %s"), *Target.ToCompactString());
	OrderStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Target);
	bMoving = true;

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | ORDER %s"), *LastOrder);
	return true;
}

bool AViperTestPilotController::OrderGoToActor(AActor* Target, float AcceptanceRadius)
{
	if (!Target || !GetPawn())
	{
		return false;
	}

	OrderTarget = Target;
	LastOrder = FString::Printf(TEXT("walk to %s"), *Target->GetName());
	OrderStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Target);
	bMoving = true;

	// Whether the move was actually taken up. SimpleMoveTo reports most of its refusals to the
	// PIE message log, which a headless run never shows, so the component's own state is the only
	// honest answer available here.
	const UPathFollowingComponent* Following = FindComponentByClass<UPathFollowingComponent>();
	UE_LOG(LogViperCompanion, Log, TEXT("pilot | ORDER %s, %.0f away | path component %s, status %d, allowed %d"),
		*LastOrder, FVector::Dist2D(GetPawn()->GetActorLocation(), Target->GetActorLocation()),
		Following ? TEXT("yes") : TEXT("NO"),
		Following ? static_cast<int32>(Following->GetStatus()) : -1,
		Following ? (Following->IsPathFollowingAllowed() ? 1 : 0) : -1);
	return true;
}

bool AViperTestPilotController::OrderFind(const FString& NameFilter, float AcceptanceRadius)
{
	AActor* Target = FindNearest(NameFilter);
	if (!Target)
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | nothing in the level matches '%s'"), *NameFilter);
		return false;
	}
	return OrderGoToActor(Target, AcceptanceRadius);
}

bool AViperTestPilotController::OrderFindAndInteract(const FString& NameFilter, float AcceptanceRadius)
{
	bInteractOnArrival = false;
	if (!OrderFind(NameFilter, AcceptanceRadius))
	{
		return false;
	}

	bInteractOnArrival = true;
	LastOrder += TEXT(" and interact");
	return true;
}

bool AViperTestPilotController::OrderTour(const FString& CommaSeparatedStops, bool bInteractAtEachStop)
{
	TourStops.Reset();
	TourVisited.Reset();

	// Plus signs and semicolons too, and for a reason: -ExecCmds uses the comma as its *command*
	// separator, so a comma-separated tour arrives already chopped into separate console
	// commands. Measured: "TOUR of 1 stops" for a list of four.
	FString Normalised = CommaSeparatedStops;
	Normalised.ReplaceInline(TEXT("+"), TEXT(","));
	Normalised.ReplaceInline(TEXT(";"), TEXT(","));
	Normalised.ParseIntoArray(TourStops, TEXT(","), true);
	for (FString& Stop : TourStops)
	{
		Stop.TrimStartAndEndInline();
	}
	TourStops.RemoveAll([](const FString& Stop) { return Stop.IsEmpty(); });

	if (TourStops.IsEmpty())
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | a tour needs stops, e.g. \"Companion,ViperItem\""));
		return false;
	}

	bTourInteracts = bInteractAtEachStop;
	TourIndex = -1;                 // AdvanceTour moves to the first

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | TOUR of %d stops%s: %s"),
		TourStops.Num(), bTourInteracts ? TEXT(", interacting at each") : TEXT(""),
		*FString::Join(TourStops, TEXT(" -> ")));

	AdvanceTour();
	return true;
}

void AViperTestPilotController::AdvanceTour()
{
	if (TourStops.IsEmpty())
	{
		return;
	}

	// Whatever it just reached is done with, so a tour of three crates is three crates and not
	// the nearest one three times.
	if (AActor* Reached = OrderTarget.Get())
	{
		TourVisited.AddUnique(Reached);
	}

	++TourIndex;
	if (!TourStops.IsValidIndex(TourIndex))
	{
		UE_LOG(LogViperCompanion, Log, TEXT("pilot | TOUR finished, %d stops visited"), TourVisited.Num());
		TourStops.Reset();
		TourIndex = INDEX_NONE;
		return;
	}

	const FString& Stop = TourStops[TourIndex];
	AActor* Target = FindNearestIgnoring(Stop, TourVisited);
	if (!Target)
	{
		UE_LOG(LogViperCompanion, Warning,
			TEXT("pilot | TOUR stop %d ('%s'): nothing left that matches, skipping"), TourIndex + 1, *Stop);
		AdvanceTour();
		return;
	}

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | TOUR stop %d of %d: '%s'"),
		TourIndex + 1, TourStops.Num(), *Stop);

	OrderGoToActor(Target, 150.f);
	bInteractOnArrival = bTourInteracts;
}

bool AViperTestPilotController::OrderWander(float Radius)
{
	APawn* MyPawn = GetPawn();
	UNavigationSystemV1* NavSystem = GetWorld()
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
	if (!MyPawn || !NavSystem)
	{
		return false;
	}

	FNavLocation Point;
	if (!NavSystem->GetRandomReachablePointInRadius(MyPawn->GetActorLocation(), Radius, Point))
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | nowhere to wander within %.0f"), Radius);
		return false;
	}

	return OrderGoToLocation(Point.Location, 80.f);
}

void AViperTestPilotController::AimAt(const AActor& Target)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	// The control rotation is what the companion reads to decide whether it is being looked at,
	// so turning the pawn alone would not be the same thing at all.
	const FVector ToTarget = Target.GetActorLocation() - MyPawn->GetActorLocation();
	const FRotator Facing = ToTarget.Rotation();
	SetControlRotation(Facing);
	MyPawn->SetActorRotation(FRotator(0.f, Facing.Yaw, 0.f));
}

bool AViperTestPilotController::OrderLookAt(AActor* Target, const float HoldSeconds)
{
	if (!Target || !GetPawn())
	{
		return false;
	}

	AimAt(*Target);

	if (HoldSeconds > 0.f && GetWorld())
	{
		LookTarget = Target;
		LookUntil = GetWorld()->GetTimeSeconds() + HoldSeconds;
	}
	else
	{
		LookTarget.Reset();
	}

	LastOrder = HoldSeconds > 0.f
		? FString::Printf(TEXT("look at %s for %.0fs"), *Target->GetName(), HoldSeconds)
		: FString::Printf(TEXT("look at %s"), *Target->GetName());
	UE_LOG(LogViperCompanion, Log, TEXT("pilot | ORDER %s"), *LastOrder);
	return true;
}

void AViperTestPilotController::OrderStop()
{
	StopMovement();
	bMoving = false;
	LookTarget.Reset();

	// Stop means stop: a tour left running would quietly set off again a second later.
	TourStops.Reset();
	TourIndex = INDEX_NONE;
	bInteractOnArrival = false;

	LastOrder = TEXT("standing still");
	UE_LOG(LogViperCompanion, Log, TEXT("pilot | ORDER stop"));
}

bool AViperTestPilotController::OrderInteract()
{
	AViperPlayerCharacter* PlayerPawn = Cast<AViperPlayerCharacter>(GetPawn());
	if (!PlayerPawn)
	{
		return false;
	}

	// What the character's own scan will see, said out loud: when an interact finds nothing it is
	// almost always the view point or the channel, and guessing at which cost three runs once.
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	FString AtTarget = TEXT("(no target)");
	if (const AActor* Target = OrderTarget.Get())
	{
		AtTarget = FString::Printf(TEXT("%s is %.0f away"), *Target->GetName(),
			FVector::Dist(ViewLocation, Target->GetActorLocation()));
	}

	// The same entry point the interact input uses, so recruiting the companion this way is the
	// same act a player would perform.
	const bool bDone = PlayerPawn->TriggerInteract();

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | INTERACT %s | looking from %s along %s, %s"),
		bDone ? TEXT("done") : TEXT("- nothing in range"),
		*ViewLocation.ToCompactString(), *ViewRotation.ToCompactString(), *AtTarget);
	return bDone;
}

FString AViperTestPilotController::Describe() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return TEXT("pilot with no pawn");
	}

	const float Elapsed = GetWorld()
		? static_cast<float>(GetWorld()->GetTimeSeconds() - OrderStartedAt) : 0.f;
	return FString::Printf(TEXT("%s at %s, %s for %.1fs, speed %.0f"),
		*MyPawn->GetName(), *MyPawn->GetActorLocation().ToCompactString(),
		*LastOrder, Elapsed, MyPawn->GetVelocity().Size2D());
}

void AViperTestPilotController::Land()
{
	APawn* MyPawn = GetPawn();
	UnPossess();
	if (MyPawn)
	{
		MyPawn->Destroy();
	}
	Destroy();
}

void AViperTestPilotController::GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		Super::GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	OutLocation = MyPawn->GetPawnViewLocation();
	OutRotation = GetControlRotation();
}

AActor* AViperTestPilotController::FindNearest(const FString& NameFilter) const
{
	return FindNearestIgnoring(NameFilter, TArray<TWeakObjectPtr<AActor>>());
}

AActor* AViperTestPilotController::FindNearestIgnoring(const FString& NameFilter,
	const TArray<TWeakObjectPtr<AActor>>& Ignore) const
{
	const APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World || NameFilter.IsEmpty())
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == MyPawn)
		{
			continue;
		}

		// Controllers and info actors are not things you walk over to, and their names match the
		// same filters: "Companion" found BP_CompanionAIController before the companion herself,
		// and the pilot walked fifteen metres to a controller and then had nothing in reach.
		if (Candidate->IsA<AController>() || Candidate->IsA<AInfo>())
		{
			continue;
		}

		// Already visited on this tour: otherwise "ViperItem, ViperItem" is the nearest crate
		// twice over and the tour never goes anywhere.
		if (Ignore.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& Seen)
			{ return Seen.Get() == Candidate; }))
		{
			continue;
		}

		const bool bMatches =
			Candidate->GetName().Contains(NameFilter, ESearchCase::IgnoreCase)
			|| Candidate->GetClass()->GetName().Contains(NameFilter, ESearchCase::IgnoreCase);
		if (!bMatches)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(MyPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}

	return Best;
}
