// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/ViperGASAbilityBlueprintLibrary.h"

#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GAS/ViperAbilitySystemComponent.h"


UViperAbilitySystemComponent* UViperGASAbilityBlueprintLibrary::GetViperAbilitySystemComponent(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (IAbilitySystemInterface* AbilitySytemInterface = Cast<IAbilitySystemInterface>(Actor))
	{
		return Cast<UViperAbilitySystemComponent>(AbilitySytemInterface->GetAbilitySystemComponent());
	}

	return Actor->FindComponentByClass<UViperAbilitySystemComponent>();
}

void UViperGASAbilityBlueprintLibrary::CancelAbilities(UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTagContainer WithTags, FGameplayTagContainer WithoutTags, UGameplayAbility* Ignore)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}
