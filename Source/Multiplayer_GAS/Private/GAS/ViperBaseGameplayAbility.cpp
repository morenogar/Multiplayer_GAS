// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/ViperBaseGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Player/ViperCharacter.h"
#include "Player/ViperPlayerController.h"


void UViperBaseGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	const UGameplayAbility* gapi = Spec.GetPrimaryInstance();
	const bool bIsPredicting = (gapi && gapi->GetCurrentActivationInfo().ActivationMode == EGameplayAbilityActivationMode::Predicting);

	if (ActorInfo && !Spec.IsActive() && !bIsPredicting && (ActivationPolicy == EViperAbilityActivationPolicy::OnSpawn))
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		const AActor* AvatarActor = ActorInfo->AvatarActor.Get();

		if (ASC && AvatarActor && !AvatarActor->GetTearOff() && (AvatarActor->GetLifeSpan() <= 0.0f))
		{
			const bool bIsLocalExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly);
			const bool bIsServerExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

			const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
			const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

			if (bClientShouldActivate || bServerShouldActivate)
			{
				ASC->TryActivateAbility(Spec.Handle);
			}
		}
	}
}

AViperCharacter* UViperBaseGameplayAbility::GetOwningCharacter() const
{
	return Cast<AViperCharacter>(GetActorInfo().OwnerActor);
}


AViperPlayerController* UViperBaseGameplayAbility::GetOwningPlayerController() const
{
	// Try PlayerController, but this is allowed to be null
	if (AViperPlayerController* controller = Cast<AViperPlayerController>(GetActorInfo().PlayerController.Get()))
	{
		return controller;
	}

	// Next try OwnerActor, which is guaranteed to be populated (but might not be a character)
	if (AViperCharacter* owner = GetOwningCharacter())
	{
		return Cast<AViperPlayerController>(owner->GetController());
	}

	return nullptr;
}

void UViperBaseGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	BP_OnAbilityAdded();
}

void UViperBaseGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilitySpec& Spec)
{
	if (!ActorInfo->OwnerActor.IsValid() || !IsValid(ActorInfo->OwnerActor.Get()))
	{
		return;
	}
	BP_OnAbilityRemoved();
	Super::OnRemoveAbility(ActorInfo, Spec);
}
