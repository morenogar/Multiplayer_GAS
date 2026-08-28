// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "GAS/ViperAttributeSet.h"
#include "ViperWidgetBar.generated.h"

class UViperAbilitySystemComponent;
class UProgressBar;
class UCommonTextBlock;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperWidgetBar : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	void SetAndBoundToGameplayAttribute(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAttribute& Attribute,  const FGameplayAttribute& MaxAttribute);
	
	void SetValue(float NewValue, float NewMaxValue);

	UPROPERTY(EditAnywhere, Category="Visual", BlueprintReadWrite)
	FLinearColor BarColor;
	
	UPROPERTY(VisibleAnywhere, meta=(BindWidget), BlueprintReadWrite)
	UProgressBar* ProgressBar;
	
	UPROPERTY(VisibleAnywhere, meta=(BindWidget), BlueprintReadWrite)
	UCommonTextBlock* ValueText;

	UPROPERTY(VisibleAnywhere, meta=(BindWidget), BlueprintReadWrite)
	UCommonTextBlock* MaxValueText;

private:

	float CachedValue;
	float CachedMaxValue;

	void ValueChanged(const FOnAttributeChangeData& ChangeData);
	void MaxValueChanged(const FOnAttributeChangeData& ChangeData);
};
