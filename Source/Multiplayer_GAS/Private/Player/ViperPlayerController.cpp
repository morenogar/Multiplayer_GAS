// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperPlayerController.h"

#include "GAS/ViperAbilitySystemComponent.h"
#include "Player/ViperCharacter.h"
#include "Player/ViperPlayerCharacter.h"
#include "Widgets/ViperGameplayWidget.h"
#include "Framework/ViperGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "AI/ViperCompanionAIController.h"
#include "AI/ViperCompanionCharacter.h"
#include "Player/ViperTestPilotController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Interactables/ViperClueActor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AI/ViperCompanionStateTreeNodes.h"
#include "AI/ViperCompanionTuning.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Interactables/ViperInteractableInterface.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_UILayerID_Gameplay, "UI.Layer.Gameplay");

AViperPlayerController::AViperPlayerController()
{

}

void AViperPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ViperPlayerCharacter = Cast<AViperPlayerCharacter>(InPawn);
	if(ViperPlayerCharacter)
	{
		ViperPlayerCharacter->ServerSideInit();
	}
}

void AViperPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	ViperPlayerCharacter = Cast<AViperPlayerCharacter>(InPawn);
	
	if(ViperPlayerCharacter)
	{
		ViperPlayerCharacter->ClientSideInit();
	}
}

void AViperPlayerController::SetGameplayWidget(UCommonActivatableWidget* Widget)
{
	if(!IsLocalPlayerController())
		return;
	
	if(UViperGameplayWidget* Gameplay = Cast<UViperGameplayWidget>(Widget))
		GameplayWidget = Gameplay;
}

void AViperPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);

	const AViperCharacter* ViperCharacter = Cast<AViperCharacter>(GetPawn());
	UViperAbilitySystemComponent*  AbilitySystem =  Cast<UViperAbilitySystemComponent>(ViperCharacter ? ViperCharacter->GetAbilitySystemComponent() : nullptr);

	if (AbilitySystem)
	{
		AbilitySystem->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
}

UViperAbilitySystemComponent* AViperPlayerController::GetViperAbilitySystemComponent() const
{
	const AViperPlayerCharacter* ViperCharacter = Cast<AViperPlayerCharacter>(GetPawn());
	return (ViperCharacter ? ViperCharacter->GetViperAbilitySystemComponent() : nullptr);
}

bool AViperPlayerController::CanStartGame() const
{
	// On a listen server the host's controller is the only one that is both local and authoritative.
	return HasAuthority() && IsLocalController();
}

bool AViperPlayerController::RequestStartGame(TSoftObjectPtr<UWorld> Map)
{
	if (!CanStartGame())
	{
		return false;
	}
	
	UWorld* World = GetWorld();
	AViperGameMode* ViperGameMode = World ? World->GetAuthGameMode<AViperGameMode>() : nullptr;
	return ViperGameMode && ViperGameMode->StartGame(Map);
}

void AViperPlayerController::ViperStartGame(const FString& MapName)
{
	const FString PackagePath = MapName.StartsWith(TEXT("/")) ? MapName : FString::Printf(TEXT("/Game/Maps/%s"), *MapName);
	const FSoftObjectPath MapPath(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath));
	
	if (!RequestStartGame(TSoftObjectPtr<UWorld>(MapPath)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ViperPlayerController] ViperStartGame %s failed (only the host can start the game)."), *MapName);
	}
}

void AViperPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(AViperPlayerController, Companion, COND_OwnerOnly);
}

void AViperPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The player left: free the companion so another player can recruit it.
	if (HasAuthority() && Companion)
	{
		Companion->Release();
	}
	
	Super::EndPlay(EndPlayReason);
}

void AViperPlayerController::SetCompanion(AViperCompanionCharacter* NewCompanion)
{
	if (HasAuthority())
	{
		Companion = NewCompanion;
	}
}

void AViperPlayerController::ServerRecruitCompanion_Implementation(AViperCompanionCharacter* CompanionToRecruit)
{
	const APawn* MyPawn = GetPawn();
	if (!CompanionToRecruit || !MyPawn)
	{
		return;
	}
	
	if (FVector::Dist(MyPawn->GetActorLocation(), CompanionToRecruit->GetActorLocation()) > CompanionToRecruit->GetMaxRecruitDistance())
	{
		return;
	}
	
	CompanionToRecruit->HandleRecruitRequest(this);
}

void AViperPlayerController::ServerIssueCompanionOrder_Implementation(FGameplayTag OrderTag, FVector_NetQuantize Location, AActor* TargetActor)
{
	if (!Companion || !OrderTag.MatchesTag(TAG_Companion_Order))
	{
		return;
	}
	
	if (const APawn* MyPawn = GetPawn())
	{
		if (FVector::DistSquared(MyPawn->GetActorLocation(), Location) > FMath::Square(MaxCompanionOrderDistance))
		{
			return;
		}
	}
	
	if (AViperCompanionAIController* CompanionController = Cast<AViperCompanionAIController>(Companion->GetController()))
	{
		CompanionController->IssueOrder(OrderTag, Location, TargetActor);
	}
}

void AViperPlayerController::ServerExamineClue_Implementation(AViperClueActor* Clue)
{
	const APawn* MyPawn = GetPawn();
	if (!Clue || !MyPawn)
	{
		return;
	}
	
	if (FVector::Dist(MyPawn->GetActorLocation(), Clue->GetActorLocation()) > Clue->GetMaxExamineDistance())
	{
		return;
	}
	
	Clue->NotifyExamined(this);
}

void AViperPlayerController::ClientPlayCompanionVoice_Implementation(const TSoftObjectPtr<USoundBase>& VoiceLine)
{
	if (VoiceLine.IsNull())
	{
		return;
	}
	
	TWeakObjectPtr<AViperPlayerController> WeakThis(this);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(VoiceLine.ToSoftObjectPath(), [WeakThis, VoiceLine]()
	{
		AViperPlayerController* PlayerController = WeakThis.Get();
		USoundBase* Sound = VoiceLine.Get();
		if (!PlayerController || !Sound)
		{
			return;
		}
		
		// Played locally, so only this player hears it.
		if (AViperCompanionCharacter* MyCompanion = PlayerController->GetCompanion())
		{
			UGameplayStatics::SpawnSoundAttached(Sound, MyCompanion->GetRootComponent());
		}
		else
		{
			UGameplayStatics::PlaySound2D(PlayerController, Sound);
		}
		PlayerController->BPE_OnCompanionVoice(Sound);
	});
}


/***********************************************************************/
/*                       SCRIPTED TEST DRIVING                         */
/***********************************************************************/

namespace ViperScript
{
	/** Close enough to a path corner to aim at the next one. Tighter than this and it wobbles on arrival. */
	static constexpr float CornerTolerance = 90.f;

	static const TCHAR* ActionName(EViperScriptAction Action)
	{
		switch (Action)
		{
		case EViperScriptAction::Walk:     return TEXT("walk");
		case EViperScriptAction::GoTo:     return TEXT("goto");
		case EViperScriptAction::Look:     return TEXT("look");
		case EViperScriptAction::Turn:     return TEXT("turn");
		case EViperScriptAction::Interact: return TEXT("interact");
		case EViperScriptAction::WalkLook: return TEXT("walklook");
		default:                           return TEXT("wait");
		}
	}
}

AActor* AViperPlayerController::FindScriptTarget(const FString& NameFilter) const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !GetWorld())
	{
		return nullptr;
	}

	// An empty filter means the companion, which is what a companion test aims at most often.
	if (NameFilter.IsEmpty())
	{
		return Companion;
	}

	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == MyPawn)
		{
			continue;
		}

		if (!Candidate->GetName().Contains(NameFilter) && !Candidate->GetActorNameOrLabel().Contains(NameFilter))
		{
			continue;
		}

		const float Distance = FVector::Dist(MyPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}

	return Best;
}

void AViperPlayerController::ViperList()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !GetWorld())
	{
		return;
	}

	const FVector Origin = MyPawn->GetActorLocation();
	UE_LOG(LogViperCompanion, Log, TEXT("list | player at %s"), *Origin.ToCompactString());

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == MyPawn)
		{
			continue;
		}

		const bool bCompanion = Candidate->IsA<AViperCompanionCharacter>();
		const bool bClue = Candidate->IsA<AViperClueActor>();
		const bool bInteractable = Candidate->GetClass()->ImplementsInterface(UViperInteractableInterface::StaticClass());
		if (!bCompanion && !bClue && !bInteractable)
		{
			continue;
		}

		UE_LOG(LogViperCompanion, Log, TEXT("list |   %s %s %.0f away"),
			*Candidate->GetName(),
			bCompanion ? TEXT("[companion]") : (bClue ? TEXT("[clue]") : TEXT("[interact]")),
			FVector::Dist(Origin, Candidate->GetActorLocation()));
	}
}

void AViperPlayerController::ViperQWalk(float Seconds)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::Walk;
	Step.Seconds = Seconds;
	Script.Add(Step);
}

void AViperPlayerController::ViperQWait(float Seconds)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::Wait;
	Step.Seconds = Seconds;
	Script.Add(Step);
}

void AViperPlayerController::ViperQGoTo(const FString& NameFilter)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::GoTo;
	Step.NameFilter = NameFilter;
	// A ceiling, not a target: a blocked or unreachable goal must not wedge the whole run.
	Step.Seconds = 25.f;
	Script.Add(Step);
}

void AViperPlayerController::ViperQGoToPoint(float X, float Y, float Z)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::GoTo;
	Step.Location = FVector(X, Y, Z);
	Step.Seconds = 25.f;
	Script.Add(Step);
}

void AViperPlayerController::ViperQLook(const FString& NameFilter, float Seconds)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::Look;
	Step.NameFilter = NameFilter;
	Step.Seconds = Seconds;
	Script.Add(Step);
}

void AViperPlayerController::ViperQTurn(float Yaw, float Seconds)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::Turn;
	Step.Yaw = Yaw;
	Step.Seconds = Seconds;
	Script.Add(Step);
}

void AViperPlayerController::ViperQWalkLook(const FString& NameFilter, float Seconds)
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::WalkLook;
	Step.NameFilter = NameFilter;
	Step.Seconds = Seconds;
	Script.Add(Step);
}

void AViperPlayerController::ViperQInteract()
{
	FViperScriptStep Step;
	Step.Action = EViperScriptAction::Interact;
	Step.Seconds = 0.5f;
	Script.Add(Step);
}

void AViperPlayerController::ViperQClear()
{
	Script.Reset();
	bRunningScript = false;
	ScriptIndex = -1;
	UE_LOG(LogViperCompanion, Log, TEXT("script | cleared"));
}

void AViperPlayerController::ViperQRun()
{
	if (!GetPawn())
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("script | no pawn to drive"));
		return;
	}

	if (Script.Num() == 0)
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("script | nothing queued"));
		return;
	}

	// Recruit whoever is nearest when the player has nobody, so a companion test is not quietly
	// measuring an idle companion that was never following in the first place.
	if (!Companion)
	{
		RehearsalCompanion = Cast<AViperCompanionCharacter>(FindScriptTarget(TEXT("Companion")));
		if (RehearsalCompanion && HasAuthority())
		{
			RehearsalCompanion->HandleRecruitRequest(this);
		}
	}

	UE_LOG(LogViperCompanion, Log, TEXT("script | START %d steps, companion=%s"),
		Script.Num(), Companion ? TEXT("yes") : TEXT("none"));

	bRunningScript = true;
	BeginScriptStep(0);
}

void AViperPlayerController::ViperRehearse()
{
	ViperQClear();
	ViperQWait(2.f);
	ViperQWalk(10.f);
	ViperQWait(4.f);
	ViperQLook(TEXT(""), 6.f);
	ViperQTurn(GetControlRotation().Yaw + 110.f, 5.f);
	ViperQWalk(8.f);
	ViperQWait(14.f);
	ViperQRun();
}

void AViperPlayerController::ViperRehearseStop()
{
	ViperQClear();
	UE_LOG(LogViperCompanion, Log, TEXT("script | STOPPED by hand"));
}

/***********************************************************************/
/*                            TEST PILOT                               */
/***********************************************************************/

void AViperPlayerController::ViperPlayAs(const FString& Mode)
{
	const FString Wanted = Mode.TrimStartAndEnd().ToLower();

	if (Wanted == TEXT("player") || Wanted.IsEmpty())
	{
		// Yours again: any pilot and its pawn go, so there is only one detective in the level.
		ViperPilotEnd();
		UE_LOG(LogViperCompanion, Log, TEXT("pilot | PLAY AS PLAYER: the pawn is yours"));
		return;
	}

	if (Wanted != TEXT("ai"))
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | ViperPlayAs takes 'player' or 'ai'"));
		return;
	}

	ViperPilotStart();

	const TArray<AViperTestPilotController*> Pilots = AViperTestPilotController::All(GetWorld());
	if (Pilots.IsEmpty())
	{
		return;                       // ViperPilotStart said why
	}

	// Recruit her first, then work through the clues. Stops that find nothing left are skipped
	// with a warning, so asking for more clues than the level holds costs nothing.
	const FString Round = TEXT("Companion+ViperClueActor+ViperClueActor+ViperClueActor")
		TEXT("+ViperClueActor+ViperClueActor+ViperClueActor");

	for (AViperTestPilotController* Pilot : Pilots)
	{
		Pilot->OrderTour(Round, true);
	}

	UE_LOG(LogViperCompanion, Log, TEXT("pilot | PLAY AS AI: recruiting, then round the clues"));
}

void AViperPlayerController::ViperPilotStart()
{
	APawn* MyPawn = GetPawn();
	const FVector Where = MyPawn
		? MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * 300.f
		: FVector::ZeroVector;

	if (AViperTestPilotController* Pilot = AViperTestPilotController::Launch(GetWorld(), Where))
	{
		UE_LOG(LogViperCompanion, Log, TEXT("pilot | started: %s"), *Pilot->Describe());
	}
	else
	{
		UE_LOG(LogViperCompanion, Warning,
			TEXT("pilot | could not start one. Is there a navmesh here, and a default pawn class?"));
	}
}

void AViperPlayerController::ViperPilotGoTo(const FString& NameFilter)
{
	const TArray<AViperTestPilotController*> Pilots = AViperTestPilotController::All(GetWorld());
	if (Pilots.IsEmpty())
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | none flying. ViperPilotStart first."));
		return;
	}

	for (AViperTestPilotController* Pilot : Pilots)
	{
		Pilot->OrderFind(NameFilter);
	}
}

void AViperPlayerController::ViperPilotRecruit(const FString& NameFilter)
{
	const TArray<AViperTestPilotController*> Pilots = AViperTestPilotController::All(GetWorld());
	if (Pilots.IsEmpty())
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | none flying. ViperPilotStart first."));
		return;
	}

	for (AViperTestPilotController* Pilot : Pilots)
	{
		Pilot->OrderFindAndInteract(NameFilter);
	}
}

void AViperPlayerController::ViperPilotTour(const FString& Stops, bool bInteractAtEachStop)
{
	const TArray<AViperTestPilotController*> Pilots = AViperTestPilotController::All(GetWorld());
	if (Pilots.IsEmpty())
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("pilot | none flying. ViperPilotStart first."));
		return;
	}

	for (AViperTestPilotController* Pilot : Pilots)
	{
		Pilot->OrderTour(Stops, bInteractAtEachStop);
	}
}

void AViperPlayerController::ViperPilotWander(float Radius)
{
	for (AViperTestPilotController* Pilot : AViperTestPilotController::All(GetWorld()))
	{
		Pilot->OrderWander(Radius);
	}
}

void AViperPlayerController::ViperAfter(const float Seconds, const FString& Command)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || Command.IsEmpty())
	{
		return;
	}

	const FString Delayed = Command.Replace(TEXT("|"), TEXT(" "));
	UE_LOG(LogViperCompanion, Log, TEXT("after | in %.1fs: %s"), Seconds, *Delayed);

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Delayed]()
	{
		UE_LOG(LogViperCompanion, Log, TEXT("after | now: %s"), *Delayed);
		ConsoleCommand(Delayed);
	}), FMath::Max(Seconds, 0.01f), false);
#endif
}

void AViperPlayerController::ViperPilotLook(const FString& NameFilter, const float Seconds)
{
	for (AViperTestPilotController* Pilot : AViperTestPilotController::All(GetWorld()))
	{
		// Reuses the pilot's own search, so "look at the companion" needs no exact name either.
		if (AActor* Target = Pilot->FindNearest(NameFilter))
		{
			Pilot->OrderLookAt(Target, Seconds);
		}
		else
		{
			UE_LOG(LogViperCompanion, Warning, TEXT("pilot | nothing matches '%s' to look at"), *NameFilter);
		}
	}
}

void AViperPlayerController::ViperPilotInteract()
{
	for (AViperTestPilotController* Pilot : AViperTestPilotController::All(GetWorld()))
	{
		Pilot->OrderInteract();
	}
}

void AViperPlayerController::ViperPilotStop()
{
	for (AViperTestPilotController* Pilot : AViperTestPilotController::All(GetWorld()))
	{
		Pilot->OrderStop();
	}
}

void AViperPlayerController::ViperPilotStatus()
{
	const TArray<AViperTestPilotController*> Pilots = AViperTestPilotController::All(GetWorld());
	if (Pilots.IsEmpty())
	{
		UE_LOG(LogViperCompanion, Log, TEXT("pilot | none flying."));
		return;
	}

	for (const AViperTestPilotController* Pilot : Pilots)
	{
		UE_LOG(LogViperCompanion, Log, TEXT("pilot | %s"), *Pilot->Describe());
	}
}

void AViperPlayerController::ViperPilotEnd()
{
	int32 Landed = 0;
	for (AViperTestPilotController* Pilot : AViperTestPilotController::All(GetWorld()))
	{
		Pilot->Land();
		++Landed;
	}
	UE_LOG(LogViperCompanion, Log, TEXT("pilot | landed %d"), Landed);
}

/***********************************************************************/
/*                           LIVE TUNING                               */
/***********************************************************************/

void AViperPlayerController::ViperTune(const FString& ParameterName, float Value)
{
	UViperCompanionTuning* Tuning = UViperCompanionTuning::Get();
	if (!Tuning)
	{
		return;
	}

	if (!Tuning->SetOverride(FName(*ParameterName), Value))
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("tune | run ViperTuneList to see the names that exist"));
	}
}

void AViperPlayerController::ViperTuneList()
{
	const UViperCompanionTuning* Tuning = UViperCompanionTuning::Get();
	if (!Tuning)
	{
		return;
	}

	UE_LOG(LogViperCompanion, Log, TEXT("tune | overriding: %s, %d in force"),
		Tuning->bEnabled ? TEXT("on") : TEXT("off"), Tuning->Overrides.Num());

	for (const TPair<FName, float>& Override : Tuning->Overrides)
	{
		UE_LOG(LogViperCompanion, Log, TEXT("tune |   %-32s = %g"), *Override.Key.ToString(), Override.Value);
	}

	// The names, not a guess at them: they come from the task struct by reflection, so this list
	// cannot drift away from what can actually be set.
	FString Available;
	for (const FName& Name : UViperCompanionTuning::ListParameters())
	{
		Available += (Available.IsEmpty() ? TEXT("") : TEXT(", "));
		Available += Name.ToString();
	}
	UE_LOG(LogViperCompanion, Log, TEXT("tune | can be set: %s"), *Available);
}

void AViperPlayerController::ViperTuneReset(const FString& ParameterName)
{
	UViperCompanionTuning* Tuning = UViperCompanionTuning::Get();
	if (!Tuning)
	{
		return;
	}

	if (ParameterName.IsEmpty())
	{
		Tuning->ClearAll();
		return;
	}

	if (!Tuning->ClearOverride(FName(*ParameterName)))
	{
		UE_LOG(LogViperCompanion, Warning, TEXT("tune | '%s' was not being overridden"), *ParameterName);
	}
}

void AViperPlayerController::ViperTuneSave()
{
	if (UViperCompanionTuning* Tuning = UViperCompanionTuning::Get())
	{
		Tuning->SaveToConfig();
	}
}

void AViperPlayerController::BeginScriptStep(int32 Index)
{
	ScriptIndex = Index;
	StepTime = 0.f;
	PathPoints.Reset();
	PathIndex = 0;
	ScriptTraceCountdown = 0.f;
	StepStartLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;

	if (!Script.IsValidIndex(Index))
	{
		bRunningScript = false;
		UE_LOG(LogViperCompanion, Log, TEXT("script | DONE"));
		return;
	}

	FViperScriptStep& Step = Script[Index];
	APawn* MyPawn = GetPawn();

	if (Step.Action == EViperScriptAction::Turn)
	{
		HeldYaw = Step.Yaw;
	}
	else if (Step.Action == EViperScriptAction::WalkLook)
	{
		// Bank the heading now: the view is about to swing onto the target, and walking "forwards"
		// would otherwise mean walking at whoever is being looked at.
		HeldYaw = GetControlRotation().Yaw;
	}
	else if (Step.Action == EViperScriptAction::GoTo && MyPawn)
	{
		// Resolved now rather than when queued: the level is not settled at queue time, and an actor
		// that spawns during the run would otherwise never be found.
		FVector Destination = Step.Location;
		if (!Step.NameFilter.IsEmpty())
		{
			const AActor* Target = FindScriptTarget(Step.NameFilter);
			if (!Target)
			{
				UE_LOG(LogViperCompanion, Warning, TEXT("script | step %d goto [%s]: nothing by that name"),
					Index, *Step.NameFilter);
				BeginScriptStep(Index + 1);
				return;
			}
			Destination = Target->GetActorLocation();
		}

		Step.Location = Destination;

		// Pathed, not steered straight: this is what walks round a wall instead of into it.
		if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			if (const UNavigationPath* Path = NavSystem->FindPathToLocationSynchronously(GetWorld(), MyPawn->GetActorLocation(), Destination))
			{
				PathPoints = Path->PathPoints;
			}
		}

		if (PathPoints.Num() == 0)
		{
			// No navmesh or no route: fall back on a straight line so the step still does something.
			PathPoints.Add(Destination);
		}

		// The first point is where it already stands.
		PathIndex = PathPoints.Num() > 1 ? 1 : 0;

		UE_LOG(LogViperCompanion, Log, TEXT("script |   path has %d corners to %s"),
			PathPoints.Num(), *Destination.ToCompactString());
	}

	UE_LOG(LogViperCompanion, Log, TEXT("script | STEP %d [%s] target=[%s] cap=%.1fs"),
		Index, ViperScript::ActionName(Step.Action), *Step.NameFilter, Step.Seconds);
}

bool AViperPlayerController::TickGoToStep(APawn& MyPawn, float DeltaTime)
{
	if (!PathPoints.IsValidIndex(PathIndex))
	{
		return true;
	}

	const FVector Here = MyPawn.GetActorLocation();
	FVector Corner = PathPoints[PathIndex];
	Corner.Z = Here.Z;

	if (FVector::Dist2D(Here, Corner) <= ViperScript::CornerTolerance)
	{
		++PathIndex;
		if (!PathPoints.IsValidIndex(PathIndex))
		{
			return true;
		}
		Corner = PathPoints[PathIndex];
		Corner.Z = Here.Z;
	}

	const FVector Direction = (Corner - Here).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		// Face the way it walks, so the view the companion reacts to is the view a player would have.
		SetControlRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		MyPawn.AddMovementInput(Direction, 1.f);
	}

	return false;
}

void AViperPlayerController::TickScript(float DeltaTime)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Script.IsValidIndex(ScriptIndex))
	{
		bRunningScript = false;
		return;
	}

	const FViperScriptStep& Step = Script[ScriptIndex];
	StepTime += DeltaTime;
	bool bFinished = false;

	// The driver reports its own speed, because inferring it from the companion trace is guesswork:
	// a frozen aim error reads the same whether nobody moved or both moved together. A step that asked
	// to walk and covered nothing is a wall, not a behaviour, and it has cost two runs already.
	ScriptTraceCountdown -= DeltaTime;
	if (ScriptTraceCountdown <= 0.f)
	{
		ScriptTraceCountdown = 0.5f;
		UE_LOG(LogViperCompanion, Log, TEXT("script |   driver spd=%.0f covered=%.0f"),
			MyPawn->GetVelocity().Size2D(), FVector::Dist2D(StepStartLocation, MyPawn->GetActorLocation()));
	}

	switch (Step.Action)
	{
	case EViperScriptAction::Walk:
	{
		const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
		MyPawn->AddMovementInput(YawOnly.Vector(), 1.f);
		bFinished = StepTime >= Step.Seconds;
		break;
	}

	case EViperScriptAction::GoTo:
		bFinished = TickGoToStep(*MyPawn, DeltaTime) || StepTime >= Step.Seconds;
		break;

	case EViperScriptAction::Look:
	{
		const AActor* Target = FindScriptTarget(Step.NameFilter);
		if (Target)
		{
			const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
			SetControlRotation((Aim - MyPawn->GetPawnViewLocation()).Rotation());
		}
		bFinished = StepTime >= Step.Seconds;
		break;
	}

	case EViperScriptAction::Turn:
		SetControlRotation(FRotator(0.f, HeldYaw, 0.f));
		bFinished = StepTime >= Step.Seconds;
		break;

	case EViperScriptAction::WalkLook:
	{
		// Movement follows the banked heading, the view follows the target: the two are decoupled on
		// purpose, which is the whole point of the step.
		const FRotator WalkDirection(0.f, HeldYaw, 0.f);
		MyPawn->AddMovementInput(WalkDirection.Vector(), 1.f);

		if (const AActor* Target = FindScriptTarget(Step.NameFilter))
		{
			const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
			SetControlRotation((Aim - MyPawn->GetPawnViewLocation()).Rotation());
		}

		bFinished = StepTime >= Step.Seconds;
		break;
	}

	case EViperScriptAction::Interact:
		if (StepTime <= DeltaTime)
		{
			if (AViperPlayerCharacter* PlayerCharacter = Cast<AViperPlayerCharacter>(MyPawn))
			{
				const AActor* Target = PlayerCharacter->GetCurrentInteractable();
				const bool bDone = PlayerCharacter->TriggerInteract();
				UE_LOG(LogViperCompanion, Log, TEXT("script |   interact %s with %s"),
					bDone ? TEXT("OK") : TEXT("FAILED, nothing in range"),
					Target ? *Target->GetName() : TEXT("nothing"));
			}
		}
		bFinished = StepTime >= Step.Seconds;
		break;

	default:
		bFinished = StepTime >= Step.Seconds;
		break;
	}

	if (bFinished)
	{
		BeginScriptStep(ScriptIndex + 1);
	}
}

void AViperPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRunningScript)
	{
		TickScript(DeltaTime);
	}
}
