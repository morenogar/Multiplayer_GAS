// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperClueSubsystem.h"

#include "Interactables/ViperClueActor.h"

void UViperClueSubsystem::RegisterClue(AViperClueActor* Clue)
{
	if (Clue)
	{
		Clues.AddUnique(Clue);
	}
}

void UViperClueSubsystem::UnregisterClue(AViperClueActor* Clue)
{
	Clues.Remove(Clue);
}

AViperClueActor* UViperClueSubsystem::FindClosestClue(const FVector& Location, float MaxDistance, TFunctionRef<bool(const AViperClueActor&)> Filter) const
{
	AViperClueActor* BestClue = nullptr;
	double BestDistanceSquared = MaxDistance > 0.f ? FMath::Square(static_cast<double>(MaxDistance)) : TNumericLimits<double>::Max();

	for (AViperClueActor* Clue : Clues)
	{
		if (!IsValid(Clue))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(Location, Clue->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared && Filter(*Clue))
		{
			BestDistanceSquared = DistanceSquared;
			BestClue = Clue;
		}
	}
	return BestClue;
}
