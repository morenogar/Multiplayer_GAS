// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/ViperAbilitySystemComponent.h"

#include "GAS/ViperAbilitySet.h"
#include "GAS/ViperBaseGameplayAbility.h"


void UViperAbilitySystemComponent::ApplyInitialEffects()
{
	if(!GetOwner() || !GetOwner()->HasAuthority())
		return;
	
	for (const TSubclassOf<UGameplayEffect>& EffectClass: InitialEffects)
	{
		FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingSpec(EffectClass, 1, MakeEffectContext());
		ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	}
}

void UViperAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UViperAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UViperAbilitySystemComponent::GrantDefaultAbilities(UViperAbilitySet* AbilitySet)
{
	if (bHasGrantedDefaultAbilities)
	{
		return;
	}
	
	if (AbilitySet)
	{
		AbilitySet->GiveToAbilitySystem(this, nullptr);
	}

	bHasGrantedDefaultAbilities = true;
}

void UViperAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();
	

	// Handle abilities that had their input pressed this frame.
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->InputPressed)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is pressed, pass input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const UViperBaseGameplayAbility* ViperGameplayAbility = CastChecked<UViperBaseGameplayAbility>(AbilitySpec->Ability);

					if (ViperGameplayAbility->GetActivationPolicy() == EViperAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	// Try to activate all the abilities that are from either presses or holds.
	// All done here so held inputs don't activate from both the hold and press.
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	// Handle all the abilities that had their input released this frame.
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;
				if (AbilitySpec->IsActive())
				{
					// Ability is active, pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	// clear the cached handles
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}
