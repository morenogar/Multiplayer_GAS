// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/ViperObjectDefinition.h"

FText UViperObjectDefinition::GetDisplayName() const
{
	return DisplayName;
}

FText UViperObjectDefinition::GetShortDescription() const
{
	return Description;
}

const FGameplayTagContainer& UViperObjectDefinition::GetGameplayTags() const
{
	return CategoryTags;
}

TSoftObjectPtr<UTexture2D> UViperObjectDefinition::GeIconSoftObject() const
{
	return Icon;
}
