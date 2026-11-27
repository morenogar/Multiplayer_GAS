// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/ViperCompanionTuning.h"

#include "AI/ViperCompanionStateTreeNodes.h"
#include "GameFramework/CharacterMovementComponent.h"

UViperCompanionTuning* UViperCompanionTuning::Get()
{
	return GetMutableDefault<UViperCompanionTuning>();
}

FName UViperCompanionTuning::GetCategoryName() const
{
	return TEXT("Viper");
}

bool UViperCompanionTuning::IsKnownParameter(FName Name)
{
	if (Name.IsNone())
	{
		return false;
	}

	// The follow task owns most of them.
	if (const UStruct* TaskStruct = FViperFollowLeaderTaskInstanceData::StaticStruct())
	{
		if (const FProperty* Property = TaskStruct->FindPropertyByName(Name))
		{
			if (Property->IsA<FFloatProperty>() || Property->IsA<FBoolProperty>())
			{
				return true;
			}
		}
	}

	// Acceleration, braking and the turn rate live on the movement component instead.
	if (const FProperty* Property = UCharacterMovementComponent::StaticClass()->FindPropertyByName(Name))
	{
		return Property->IsA<FFloatProperty>() || Property->IsA<FBoolProperty>() || Name == TEXT("RotationRate");
	}

	return false;
}

TArray<FName> UViperCompanionTuning::ListParameters()
{
	TArray<FName> Names;

	if (const UStruct* TaskStruct = FViperFollowLeaderTaskInstanceData::StaticStruct())
	{
		for (TFieldIterator<FProperty> It(TaskStruct); It; ++It)
		{
			// EditAnywhere only: the internal running state is not something to hand a slider.
			if (!It->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}
			if (It->IsA<FFloatProperty>() || It->IsA<FBoolProperty>())
			{
				Names.Add(It->GetFName());
			}
		}
	}

	// The three that belong to the body rather than to the decision.
	Names.Add(TEXT("MaxAcceleration"));
	Names.Add(TEXT("BrakingDecelerationWalking"));
	Names.Add(TEXT("RotationRate"));
	return Names;
}

bool UViperCompanionTuning::SetOverride(FName Name, float Value)
{
	if (!IsKnownParameter(Name))
	{
		UE_LOG(LogViperCompanion, Warning,
			TEXT("tune | no parameter called '%s' - check the spelling against the Viper Follow Leader task"),
			*Name.ToString());
		return false;
	}

	Overrides.Add(Name, Value);

	// Setting a value is the whole point, so do not also ask the user to remember to switch it on.
	bEnabled = true;

	UE_LOG(LogViperCompanion, Log, TEXT("tune | %s = %g"), *Name.ToString(), Value);
	return true;
}

bool UViperCompanionTuning::ClearOverride(FName Name)
{
	const bool bRemoved = Overrides.Remove(Name) > 0;
	UE_CLOG(bRemoved, LogViperCompanion, Log, TEXT("tune | %s back to the asset value"), *Name.ToString());
	return bRemoved;
}

void UViperCompanionTuning::ClearAll()
{
	Overrides.Empty();
	bEnabled = false;
	UE_LOG(LogViperCompanion, Log, TEXT("tune | cleared, the asset is in charge again"));
}

void UViperCompanionTuning::SaveToConfig()
{
	TryUpdateDefaultConfigFile();
	UE_LOG(LogViperCompanion, Log, TEXT("tune | saved %d overrides to the project config"), Overrides.Num());
}
