// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/ViperAbilitySet.h"

#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperBaseGameplayAbility.h"


void FViperAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FViperAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FViperAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (Set != nullptr)
	GrantedAttributeSets.Add(Set);
}

void FViperAbilitySet_GrantedHandles::AddAbility(UViperAbilitySystemComponent* ViperAbilitySystemComponent, TSubclassOf<UViperBaseGameplayAbility> Ability, FGameplayTag InputTag)
{
	check(ViperAbilitySystemComponent);
	check(Ability);
	
	if (!ViperAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}
	UViperBaseGameplayAbility* AbilityCDO = Ability->GetDefaultObject<UViperBaseGameplayAbility>();

	FGameplayAbilitySpec AbilitySpec(AbilityCDO, 1);
	AbilitySpec.SourceObject = nullptr;

	if (InputTag.IsValid())
	{
		AbilitySpec.DynamicAbilityTags.AddTag(InputTag);
	}

	const FGameplayAbilitySpecHandle AbilitySpecHandle = ViperAbilitySystemComponent->GiveAbility(AbilitySpec);

	AddAbilitySpecHandle(AbilitySpecHandle);
}

UViperAbilitySet::UViperAbilitySet(const FObjectInitializer& ObjectInitializer)
{
}

void UViperAbilitySet::GiveToAbilitySystem(UViperAbilitySystemComponent* ViperAbilitySystemComponent, FViperAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
		check(ViperAbilitySystemComponent);

	if (!ViperAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		// must be authoritative to give or take ability sets.
		return;
	}
	
	// Grant the gameplay abilities thrid
	for (int32 AbilityIndex = 0; AbilityIndex < GrantedGameplayAbilities.Num(); ++AbilityIndex)
	{
		const FViperAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[AbilityIndex];

		if (!IsValid(AbilityToGrant.Ability))
		{
			UE_LOG(LogAbilitySystemComponent, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		UViperBaseGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<UViperBaseGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		AbilitySpec.DynamicAbilityTags.AddTag(AbilityToGrant.InputTag);

		const FGameplayAbilitySpecHandle AbilitySpecHandle = ViperAbilitySystemComponent->GiveAbility(AbilitySpec);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}

		UViperBaseGameplayAbility* ViperGameplayAbility = Cast<UViperBaseGameplayAbility>(AbilityCDO);

		if (ViperGameplayAbility && ViperGameplayAbility->GetActivationPolicy() == EViperAbilityActivationPolicy::OnGranted)
		{
			ViperAbilitySystemComponent->TryActivateAbility(AbilitySpecHandle);
		}
	}
}
