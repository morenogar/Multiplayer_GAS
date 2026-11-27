// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/ViperCompanionCharacter.h"

#include "AI/ViperCompanionAIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Interactables/ViperObjectDefinitionSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/ViperPlayerController.h"
#include "TimerManager.h"

AViperCompanionCharacter::AViperCompanionCharacter()
{
	// Ticks on every machine to smooth the gaze locally (see UpdateLookAt).
	PrimaryActorTick.bCanEverTick = true;

	// Nobody predicts the companion abilities: Minimal is the recommended GAS replication mode for AI.
	ViperAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AIControllerClass = AViperCompanionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	// Movement tuned so it reads as a person walking, not a turret on rails.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	// 720 deg/s is a pivot on the spot; a person leans into the turn.
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = 450.f;
	Movement->MinAnalogWalkSpeed = 20.f;
	// Eases into speed instead of snapping to the maximum on the first frame.
	Movement->MaxAcceleration = 900.f;
	Movement->bRequestedMoveUseAcceleration = true;
	// Coasts to a stop over a short distance rather than stopping dead.
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = 1.f;
	Movement->BrakingDecelerationWalking = 800.f;
	// RVO: steps around the player and other companions instead of shoving through them.
	// Keep this well under the follow task FormationDistance. At 220 it matched the slot exactly, so
	// the companion sat permanently on the edge of its own avoidance radius and was shoved away from
	// the one actor it is meant to walk beside: that is the nervous orbiting, not the pathing.
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 120.f;

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(GetRootComponent());
	InteractionCollision->SetSphereRadius(80.f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	InteractionCollision->SetGenerateOverlapEvents(true);
}

void AViperCompanionCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AViperCompanionCharacter, LeaderPlayerState);
	DOREPLIFETIME(AViperCompanionCharacter, LookAtState);
}

void AViperCompanionCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Clients never possess the companion: init the ability actor info here (the server does it in PossessedBy).
	if (!HasAuthority())
	{
		ClientSideInit();
	}
}

void AViperCompanionCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority() && ViperAbilitySystemComponent)
	{
		ServerSideInit();
		ViperAbilitySystemComponent->GrantDefaultAbilities(AbilitySets);
	}
}

void AViperCompanionCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLookAt(DeltaSeconds);
}

void AViperCompanionCharacter::Interact(APlayerController* Player)
{
	if (AViperPlayerController* ViperPlayerController = Cast<AViperPlayerController>(Player))
	{
		ViperPlayerController->ServerRecruitCompanion(this);
	}
}

const UViperObjectDefinition* AViperCompanionCharacter::GetObjectDefinition() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UViperObjectDefinitionSubsystem* ObjectDefinitionSubsystem = GameInstance->GetSubsystem<UViperObjectDefinitionSubsystem>())
			{
				return ObjectDefinitionSubsystem->GetObjectDefinition(ObjectDefinition);
			}
		}
	}
	return nullptr;
}

bool AViperCompanionCharacter::HandleRecruitRequest(AViperPlayerController* NewLeader)
{
	if (!HasAuthority() || !NewLeader)
	{
		return false;
	}

	// Interacting with your own companion dismisses it.
	if (LeaderController.Get() == NewLeader)
	{
		Release();
		return true;
	}

	// Already following someone else.
	if (LeaderController.IsValid())
	{
		return false;
	}

	// One companion per player: let the previous one go.
	if (AViperCompanionCharacter* PreviousCompanion = NewLeader->GetCompanion())
	{
		PreviousCompanion->Release();
	}

	LeaderController = NewLeader;
	LeaderPlayerState = NewLeader->PlayerState;
	NewLeader->SetCompanion(this);

	if (AViperCompanionAIController* CompanionController = GetCompanionController())
	{
		CompanionController->SetLeader(NewLeader);
	}

	// RepNotify does not run on the server (listen server host): call it by hand.
	OnRep_LeaderPlayerState();
	return true;
}

void AViperCompanionCharacter::Release()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AViperPlayerController* OldLeader = LeaderController.Get())
	{
		if (OldLeader->GetCompanion() == this)
		{
			OldLeader->SetCompanion(nullptr);
		}
	}

	LeaderController.Reset();
	LeaderPlayerState = nullptr;

	// Stop staring at a leader it no longer has.
	ClearLookAt();
	SetSpeaking(false);

	if (AViperCompanionAIController* CompanionController = GetCompanionController())
	{
		CompanionController->SetLeader(nullptr);
	}

	OnRep_LeaderPlayerState();
}

void AViperCompanionCharacter::OnRep_LeaderPlayerState()
{
	BPE_OnLeaderChanged(LeaderPlayerState);
}

AViperCompanionAIController* AViperCompanionCharacter::GetCompanionController() const
{
	return Cast<AViperCompanionAIController>(GetController());
}

/***********************************************************************/
/*                              LOOK AT                                */
/***********************************************************************/

void AViperCompanionCharacter::SetLookAtActor(AActor* InActor, float InWeight)
{
	if (!HasAuthority())
	{
		return;
	}

	LookAtState.Actor = InActor;
	LookAtState.Location = FVector::ZeroVector;
	LookAtState.Weight = InActor ? FMath::Clamp(InWeight, 0.f, 1.f) : 0.f;
}

void AViperCompanionCharacter::SetLookAtLocation(const FVector& InLocation, float InWeight)
{
	if (!HasAuthority())
	{
		return;
	}

	LookAtState.Actor = nullptr;
	LookAtState.Location = InLocation;
	LookAtState.Weight = FMath::Clamp(InWeight, 0.f, 1.f);
}

void AViperCompanionCharacter::ClearLookAt()
{
	if (!HasAuthority())
	{
		return;
	}

	LookAtState.Actor = nullptr;
	LookAtState.Location = FVector::ZeroVector;
	LookAtState.Weight = 0.f;
}

void AViperCompanionCharacter::SetSpeaking(bool bInSpeaking, float Duration)
{
	if (!HasAuthority())
	{
		return;
	}

	LookAtState.bSpeaking = bInSpeaking;

	GetWorldTimerManager().ClearTimer(SpeakingTimerHandle);
	if (bInSpeaking && Duration > 0.f)
	{
		GetWorldTimerManager().SetTimer(SpeakingTimerHandle, this, &AViperCompanionCharacter::OnSpeakingFinished, Duration, false);
	}
}

void AViperCompanionCharacter::OnSpeakingFinished()
{
	if (HasAuthority())
	{
		LookAtState.bSpeaking = false;
	}
}

bool AViperCompanionCharacter::HasLookAtTarget() const
{
	return LookAtState.Weight > 0.f && (LookAtState.Actor != nullptr || !LookAtState.Location.IsNearlyZero());
}

FVector AViperCompanionCharacter::GetLookAtPointMeshSpace() const
{
	if (const USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		return MeshComponent->GetComponentTransform().InverseTransformPosition(CurrentLookAtPoint);
	}
	return CurrentLookAtPoint;
}

FVector AViperCompanionCharacter::GetLookAtSourceLocation() const
{
	if (const USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (!LookAtSourceBone.IsNone() && MeshComponent->DoesSocketExist(LookAtSourceBone))
		{
			return MeshComponent->GetSocketLocation(LookAtSourceBone);
		}
	}
	return GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
}

bool AViperCompanionCharacter::ResolveLookAtTarget(FVector& OutPoint) const
{
	if (LookAtState.Weight <= 0.f)
	{
		return false;
	}

	// An actor target is resolved every frame, so the gaze tracks a moving player.
	if (const AActor* TargetActor = LookAtState.Actor)
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		TargetActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		OutPoint = EyeLocation;
		return true;
	}

	if (!LookAtState.Location.IsNearlyZero())
	{
		OutPoint = LookAtState.Location;
		return true;
	}

	return false;
}

void AViperCompanionCharacter::UpdateLookAt(float DeltaSeconds)
{
	const FVector Source = GetLookAtSourceLocation();
	const FTransform ActorTransform = GetActorTransform();

	// Neutral pose: straight ahead. Used while there is nothing to look at, so the head eases back instead of popping.
	FVector DesiredPoint = Source + GetActorForwardVector() * NeutralLookAtDistance;
	float DesiredAlpha = 0.f;

	FVector TargetPoint;
	if (ResolveLookAtTarget(TargetPoint))
	{
		const FVector ToTarget = TargetPoint - Source;
		const float Distance = ToTarget.Size();
		if (Distance > KINDA_SMALL_NUMBER)
		{
			// Work in actor space, so the clamp means "how far the neck turns" and not some world angle.
			const FVector LocalDir = ActorTransform.InverseTransformVectorNoScale(ToTarget / Distance);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, FMath::Sqrt(LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y)));

			// Past the neck limit the gaze fades out over LookAtYawFalloff instead of snapping: it gives up on you.
			const float Commitment = 1.f - FMath::Clamp((FMath::Abs(Yaw) - MaxLookAtYaw) / LookAtYawFalloff, 0.f, 1.f);

			const float ClampedYaw = FMath::Clamp(Yaw, -MaxLookAtYaw, MaxLookAtYaw);
			const float ClampedPitch = FMath::Clamp(Pitch, -MaxLookAtPitch, MaxLookAtPitch);

			const FVector ClampedLocalDir = FRotator(ClampedPitch, ClampedYaw, 0.f).Vector();
			DesiredPoint = Source + ActorTransform.TransformVectorNoScale(ClampedLocalDir) * Distance;
			DesiredAlpha = FMath::Clamp(LookAtState.Weight, 0.f, 1.f) * Commitment;
		}
	}

	// First frame: snap, so a freshly spawned companion does not sweep its head in from wherever.
	if (!bLookAtInitialized)
	{
		CurrentLookAtPoint = DesiredPoint;
		bLookAtInitialized = true;
	}

	CurrentLookAtPoint = FMath::VInterpTo(CurrentLookAtPoint, DesiredPoint, DeltaSeconds, LookAtInterpSpeed);
	CurrentLookAtAlpha = FMath::FInterpTo(CurrentLookAtAlpha, DesiredAlpha, DeltaSeconds, LookAtBlendSpeed);

	// Report the smoothed gaze, not the raw target: an Aim Offset driven by these stays in sync with the head bone.
	const FVector SmoothedDir = (CurrentLookAtPoint - Source).GetSafeNormal();
	if (!SmoothedDir.IsNearlyZero())
	{
		const FVector LocalSmoothed = ActorTransform.InverseTransformVectorNoScale(SmoothedDir);
		CurrentLookAtYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalSmoothed.Y, LocalSmoothed.X));
		CurrentLookAtPitch = FMath::RadiansToDegrees(FMath::Atan2(LocalSmoothed.Z, FMath::Sqrt(LocalSmoothed.X * LocalSmoothed.X + LocalSmoothed.Y * LocalSmoothed.Y)));
	}
}
