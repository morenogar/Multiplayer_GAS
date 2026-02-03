// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ViperWidgetBar.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"
#include "GAS/ViperAbilitySystemComponent.h"

void UViperWidgetBar::NativeConstruct()
{
	Super::NativeConstruct();
}

void UViperWidgetBar::NativePreConstruct()
{
	Super::NativePreConstruct();
	ProgressBar->SetFillColorAndOpacity(BarColor);
}

void UViperWidgetBar::SetAndBoundToGameplayAttribute(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayAttribute& Attribute, const FGameplayAttribute& MaxAttribute)
{
	if(AbilitySystemComponent)
	{
		bool bFoundValue;
		bool bFoundNewValue;
		float Value = AbilitySystemComponent->GetGameplayAttributeValue(Attribute,bFoundValue);
		float MaxValue = AbilitySystemComponent->GetGameplayAttributeValue(MaxAttribute,bFoundNewValue);
		
		if(bFoundValue && bFoundNewValue)
			SetValue(Value,MaxValue);
		
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this,&UViperWidgetBar::ValueChanged);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this,&UViperWidgetBar::MaxValueChanged);
		
	}
}

void UViperWidgetBar::SetValue(float NewValue, float NewMaxValue)
{
	CachedValue = NewValue;
	CachedMaxValue = NewMaxValue;
	
	if(NewMaxValue <= 0)
	{
		UE_LOG(LogTemp, Warning,TEXT("Value of %s can not be <= 0"), *GetName());
	}

	float NewPercent = NewValue / NewMaxValue;
	ProgressBar->SetPercent(NewPercent);

	FNumberFormattingOptions FormatOptions = FNumberFormattingOptions().SetMaximumFractionalDigits(0);
	//ValueText->SetText(FText::Format(FTextFormat::FromString("{0}|{1}"),FText::AsNumber(NewValue,&FormatOptions),FText::AsNumber(NewMaxValue,&FormatOptions)));
	ValueText->SetText(FText::Format(FTextFormat::FromString("{0}"),FText::AsNumber(NewValue,&FormatOptions)));
	MaxValueText->SetText(FText::Format(FTextFormat::FromString("{0}"),FText::AsNumber(NewMaxValue,&FormatOptions)));

}

void UViperWidgetBar::ValueChanged(const FOnAttributeChangeData& ChangeData)
{
	SetValue(ChangeData.NewValue,CachedMaxValue);
}

void UViperWidgetBar::MaxValueChanged(const FOnAttributeChangeData& ChangeData)
{
	SetValue(CachedValue,ChangeData.NewValue);
}
