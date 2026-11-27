// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerController.h"
#include "NativeGameplayTags.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "ViperPlayerController.generated.h"

class UViperAbilitySystemComponent;
class UAbilitySystemComponent;
class UCommonActivatableWidget;
class UViperGameplayWidget;
class AViperCharacter;
class AViperCompanionCharacter;
class AViperClueActor;
class USoundBase;

MULTIPLAYER_GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UILayerID_Gameplay);
/** One queued step of a scripted test run. */
UENUM()
enum class EViperScriptAction : uint8
{
	Wait,
	Walk,
	GoTo,
	Look,
	Turn,
	Interact,
	/** Walks a banked heading while the view stays on a target: looking at someone as you pass them. */
	WalkLook,
};

USTRUCT()
struct FViperScriptStep
{
	GENERATED_BODY()

	UPROPERTY()
	EViperScriptAction Action = EViperScriptAction::Wait;

	UPROPERTY()
	float Seconds = 0.f;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Resolved when the step starts, not when it is queued: the level may not be settled at queue time. */
	UPROPERTY()
	FString NameFilter;
};

UCLASS()
class MULTIPLAYER_GAS_API AViperPlayerController : public APlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AViperPlayerController();

	//Only called on the server
	virtual void OnPossess(APawn* InPawn) override;

	//Only called on the client also on the listening server
	virtual void AcknowledgePossession(APawn* InPawn) override;

	UFUNCTION(BlueprintCallable)
	void SetGameplayWidget(UCommonActivatableWidget* Widget);

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintCallable, Category="Viper|PlayerController")
	UViperAbilitySystemComponent* GetViperAbilitySystemComponent() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
	{
		return GetViperAbilitySystemComponent();
	}
	
	/** True only for the host (listen server's local player). Use it to show/enable the Start Game button. */
	UFUNCTION(BlueprintPure, Category = "Viper|Session")
	bool CanStartGame() const;
	
	/** Host only: takes every player in the session to Map. Returns false on clients or if the travel can't start. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Session")
	bool RequestStartGame(TSoftObjectPtr<UWorld> Map);
	
	/** Console command for testing: "ViperStartGame Claude_Map" (name under /Game/Maps/) or a full path. */
	UFUNCTION(Exec)
	void ViperStartGame(const FString& MapName);

	/*------------------------------------------------------------------------*/
	/*  Scripted driving: console commands that queue steps, then run them.    */
	/*  Built to be passed on the command line in one go, which is what makes  */
	/*  an unattended run possible:                                            */
	/*    -ExecCmds="ViperQGoTo Clue, ViperQInteract, ViperQLook Companion,    */
	/*               ViperQWait 3, ViperQRun"                                  */
	/*------------------------------------------------------------------------*/

	/** Logs what is around: companions, clue actors and anything interactable, with names and distances. */
	UFUNCTION(Exec)
	void ViperList();

	/** Queues: walk forward for Seconds, holding the current facing. */
	UFUNCTION(Exec)
	void ViperQWalk(float Seconds = 5.f);

	/** Queues: stand still for Seconds. */
	UFUNCTION(Exec)
	void ViperQWait(float Seconds = 2.f);

	/** Queues: path to the first actor whose name contains NameFilter, going round whatever is in the way. */
	UFUNCTION(Exec)
	void ViperQGoTo(const FString& NameFilter);

	/** Queues: path to a world point. */
	UFUNCTION(Exec)
	void ViperQGoToPoint(float X, float Y, float Z);

	/** Queues: aim the view at the first actor whose name contains NameFilter, and hold it for Seconds. */
	UFUNCTION(Exec)
	void ViperQLook(const FString& NameFilter, float Seconds = 3.f);

	/** Queues: turn to an absolute yaw and hold for Seconds. */
	UFUNCTION(Exec)
	void ViperQTurn(float Yaw, float Seconds = 1.f);

	/**
	 * Queues: keep walking the heading held at the start of the step while the view follows a target.
	 * This is the case that tells us whether the companion eases off mid-stride or stops dead, which
	 * a plain Look cannot: by then the player has already come to a halt and there is nothing to slow.
	 */
	UFUNCTION(Exec)
	void ViperQWalkLook(const FString& NameFilter, float Seconds = 6.f);

	/** Queues: interact with whatever is in range, the same call the input binding makes. */
	UFUNCTION(Exec)
	void ViperQInteract();

	/** Runs the queue. Each step names itself in the log, so a trace reads as a timeline. */
	UFUNCTION(Exec)
	void ViperQRun();

	UFUNCTION(Exec)
	void ViperQClear();

	/** Queues the stock companion routine - walk, stop, look, look away, walk, walk while looking, wait. */
	UFUNCTION(Exec)
	void ViperRehearse();

	/*------------------------------------------------------------------------*/
	/*  The test pilot: an AI that plays the player.                          */
	/*                                                                        */
	/*  The ViperQ steps above replay a fixed list of corners. These hand a   */
	/*  pawn to an AViperTestPilotController, which is told what to achieve   */
	/*  and works the route out itself - so a map it has never seen is a fair */
	/*  test rather than a recording, and "go and find the thing" becomes one */
	/*  order instead of a list of waypoints someone had to measure first.    */
	/*------------------------------------------------------------------------*/

	/**
	 * Who plays: "player" leaves the pawn to you, "ai" hands a pawn to the test pilot and sets it
	 * off round the clues, recruiting the companion on the way. The same choice the browser lab
	 * carries in its export, so a map built for one is played as that one.
	 */
	UFUNCTION(Exec)
	void ViperPlayAs(const FString& Mode);

	/** Spawns a second player character near you and hands it to an AI. */
	UFUNCTION(Exec)
	void ViperPilotStart();

	/** Sends the pilot to the nearest actor whose name or class contains the filter. */
	UFUNCTION(Exec)
	void ViperPilotGoTo(const FString& NameFilter);

	/** Walk to the nearest match and interact on arrival: recruiting the companion in one order. */
	UFUNCTION(Exec)
	void ViperPilotRecruit(const FString& NameFilter = TEXT("Companion"));

	/**
	 * A round of stops for the pilot, comma separated, e.g.
	 *   ViperPilotTour Companion,ViperItem,ViperItem 1
	 * The second argument makes it interact at each stop, which is how one line recruits the
	 * companion and then walks her round the level. Needed because -ExecCmds fires every command
	 * in the same frame, so a route cannot be written as several commands.
	 */
	UFUNCTION(Exec)
	void ViperPilotTour(const FString& Stops, bool bInteractAtEachStop = false);

	UFUNCTION(Exec)
	void ViperPilotWander(float Radius = 1200.f);

	/**
	 * Turns the pilot's view to something, which is what the companion reads as being looked at.
	 * With Seconds > 0 it keeps looking (re-aiming every frame) for that long.
	 */
	UFUNCTION(Exec)
	void ViperPilotLook(const FString& NameFilter, float Seconds = 0.f);

	/**
	 * Runs a console command after a delay, so a scenario can say "then, a minute later, ...":
	 * -ExecCmds fires every command in the same frame and splits on commas, and quotes do not
	 * survive the shell, so spaces inside the delayed command are written as '|':
	 *   ViperAfter 60 ViperPilotLook|BP_Companion_C_2|30
	 * Test tooling; not compiled into Shipping.
	 */
	UFUNCTION(Exec)
	void ViperAfter(float Seconds, const FString& Command);

	UFUNCTION(Exec)
	void ViperPilotInteract();

	UFUNCTION(Exec)
	void ViperPilotStop();

	/** Prints what every pilot is doing. */
	UFUNCTION(Exec)
	void ViperPilotStatus();

	/** Removes every pilot and its pawn. */
	UFUNCTION(Exec)
	void ViperPilotEnd();

	UFUNCTION(Exec)
	void ViperRehearseStop();

	/*------------------------------------------------------------------------*/
	/*  Live tuning of the companion walk.                                     */
	/*  The parameters live inside ST_Companion, so changing one normally       */
	/*  means edit, compile and restart. These write UViperCompanionTuning      */
	/*  instead, which the follow task reads every tick, so a value can be      */
	/*  judged while the walk is happening - and in a packaged build, where     */
	/*  there is no StateTree editor at all.                                    */
	/*------------------------------------------------------------------------*/

	/** ViperTune FormationDistance 260. Booleans take 0 or 1. Reports an unknown name. */
	UFUNCTION(Exec)
	void ViperTune(const FString& ParameterName, float Value);

	/** Prints the overrides in force, and every name that can be overridden. */
	UFUNCTION(Exec)
	void ViperTuneList();

	/** Drops one override, or all of them when given no name, so the asset is in charge again. */
	UFUNCTION(Exec)
	void ViperTuneReset(const FString& ParameterName);

	/** Writes the overrides into Config/DefaultGame.ini so they survive a restart. */
	UFUNCTION(Exec)
	void ViperTuneSave();

	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/***********************************************************************/
	/*                              COMPANION                              */
	/***********************************************************************/
	
	/** This player's companion (replicated to the owning client only). */
	UFUNCTION(BlueprintPure, Category = "Viper|Companion")
	AViperCompanionCharacter* GetCompanion() const { return Companion; }
	
	/** Server. Called by the companion when it's recruited / dismissed. */
	void SetCompanion(AViperCompanionCharacter* NewCompanion);
	
	/** Recruits (or dismisses, if it's already ours) the companion. Sent by the companion's Interact. */
	UFUNCTION(Server, Reliable)
	void ServerRecruitCompanion(AViperCompanionCharacter* CompanionToRecruit);
	
	/** Gives a Companion.Order.* order. Location / TargetActor: where the player was aiming. */
	UFUNCTION(Server, Reliable)
	void ServerIssueCompanionOrder(FGameplayTag OrderTag, FVector_NetQuantize Location, AActor* TargetActor);
	
	/** Plays a line of the companion's voice. Only this player hears it (attached to their companion). */
	UFUNCTION(Client, Reliable)
	void ClientPlayCompanionVoice(const TSoftObjectPtr<USoundBase>& VoiceLine);
	
	/***********************************************************************/
	/*                               CLUES                                 */
	/***********************************************************************/
	
	/** Examines a clue (sent by the clue's Interact): validates the distance and fires the clue's On Examined. */
	UFUNCTION(Server, Reliable)
	void ServerExamineClue(AViperClueActor* Clue);
	
protected:
	/** Called on the owning client after a companion voice line starts. Use it for subtitles or to animate the UI. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Viper|Companion", meta = (DisplayName = "On Companion Voice"))
	void BPE_OnCompanionVoice(USoundBase* VoiceLine);
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** Orders aimed farther than this from the player's pawn are ignored by the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion", meta = (ClampMin = "0"))
	float MaxCompanionOrderDistance = 6000.f;

private:
	UPROPERTY()
	AViperCharacter* ViperPlayerCharacter;

	UPROPERTY()
	UViperGameplayWidget* GameplayWidget;
	
	UPROPERTY(Replicated)
	TObjectPtr<AViperCompanionCharacter> Companion;

	/*------------------------- Scripted driving -------------------------------*/

	UPROPERTY()
	TArray<FViperScriptStep> Script;

	UPROPERTY()
	bool bRunningScript = false;

	UPROPERTY()
	int32 ScriptIndex = -1;

	/** Seconds spent in the current step, not since the script began. */
	UPROPERTY()
	float StepTime = 0.f;

	/** Nav path for a GoTo step, walked corner by corner so walls are gone round rather than into. */
	UPROPERTY()
	TArray<FVector> PathPoints;

	UPROPERTY()
	int32 PathIndex = 0;

	/** Banked at the start of a Turn step so the view does not creep while the step runs. */
	UPROPERTY()
	float HeldYaw = 0.f;

	UPROPERTY()
	float ScriptTraceCountdown = 0.f;

	/** Distance covered in this step, so a step that asked to walk can report that it went nowhere. */
	UPROPERTY()
	FVector StepStartLocation = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AViperCompanionCharacter> RehearsalCompanion;

	void TickScript(float DeltaTime);
	void BeginScriptStep(int32 Index);
	bool TickGoToStep(APawn& MyPawn, float DeltaTime);

	/** First actor whose name contains the filter, nearest first. Empty filter means the companion. */
	AActor* FindScriptTarget(const FString& NameFilter) const;
};
