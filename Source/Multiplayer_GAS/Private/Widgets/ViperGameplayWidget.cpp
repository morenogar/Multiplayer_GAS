// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperGameplayWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Widgets/ViperWidgetBar.h"

void UViperGameplayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OwnerAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPlayerPawn());

	if(OwnerAbilitySystemComponent)
	{
		HealthBar->SetAndBoundToGameplayAttribute(OwnerAbilitySystemComponent,UViperAttributeSet::GetHealthAttribute(),UViperAttributeSet::GetMaxHealthAttribute());
		ManaBar->SetAndBoundToGameplayAttribute(OwnerAbilitySystemComponent,UViperAttributeSet::GetManaAttribute(),UViperAttributeSet::GetMaxManaAttribute());
	}
}
