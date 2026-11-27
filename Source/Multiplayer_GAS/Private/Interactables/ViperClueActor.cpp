// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperClueActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Interactables/ViperClueDefinition.h"
#include "Interactables/ViperClueSubsystem.h"
#include "Player/ViperPlayerController.h"

AViperClueActor::AViperClueActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Placed in the level and never changes: replicated only so it can be referenced in RPCs, then dormant.
	bReplicates = true;
	NetDormancy = DORM_Initial;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(Mesh);
	InteractionCollision->SetSphereRadius(60.f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	InteractionCollision->SetGenerateOverlapEvents(true);
}

void AViperClueActor::BeginPlay()
{
	Super::BeginPlay();

	if (UViperClueSubsystem* ClueSubsystem = GetWorld()->GetSubsystem<UViperClueSubsystem>())
	{
		ClueSubsystem->RegisterClue(this);
	}
}

void AViperClueActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UViperClueSubsystem* ClueSubsystem = GetWorld()->GetSubsystem<UViperClueSubsystem>())
	{
		ClueSubsystem->UnregisterClue(this);
	}

	Super::EndPlay(EndPlayReason);
}

const UViperObjectDefinition* AViperClueActor::GetObjectDefinition() const
{
	return ClueDefinition;
}

void AViperClueActor::Interact(APlayerController* Player)
{
	if (AViperPlayerController* ViperPlayerController = Cast<AViperPlayerController>(Player))
	{
		ViperPlayerController->ServerExamineClue(this);
	}
}

void AViperClueActor::NotifyExamined(APlayerController* ExaminingPlayer)
{
	if (HasAuthority())
	{
		BPE_OnExamined(ExaminingPlayer);
	}
}
