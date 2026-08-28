// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperAbilityActor.h"

#include "Interactables/ViperInteractResponse.h"
#include "Interactables/ViperObjectDefinitionSubsystem.h"

// Sets default values
AViperAbilityActor::AViperAbilityActor()
{
}

// Called when the game starts or when spawned
void AViperAbilityActor::BeginPlay()
{
	Super::BeginPlay();

}

const UViperObjectDefinition* AViperAbilityActor::GetObjectDefinition() const
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UViperObjectDefinitionSubsystem* ObjectDefinitionSubsystem = GameInstance->GetSubsystem<UViperObjectDefinitionSubsystem>())
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
