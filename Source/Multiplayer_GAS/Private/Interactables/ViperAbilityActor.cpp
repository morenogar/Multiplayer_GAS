// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperAbilityActor.h"

#include "Interactables/ViperInteractResponse.h"
#include "Interactables/ViperObjectDefinitionSubsystem.h"

// Sets default values
AViperAbilityActor::AViperAbilityActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AViperAbilityActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AViperAbilityActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

const UViperObjectDefinition* AViperAbilityActor::GetObjectDefinition() const
{
	if (UWorld* world = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* gameInstance = world->GetGameInstance())
		{
			if (UViperObjectDefinitionSubsystem* ObjectDefinitionSubsystem = gameInstance->GetSubsystem<UViperObjectDefinitionSubsystem>())
				return ObjectDefinitionSubsystem->GetObjectDefinition(ObjectDefinition);
		}
	}
	
	return nullptr;
}

void AViperAbilityActor::Interact(APlayerController* Player)
{
	for (UViperInteractResponse* Response : InteractResponses)
	{
		if (Response)
		{
			Response->Interact(Player);
		}
	}
}
