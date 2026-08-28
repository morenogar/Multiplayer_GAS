// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ViperSettingBaseVM.h"
#include "ViperMultiOptionSettingVM.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperMultiOptionSettingVM : public UViperSettingBaseVM
{
	GENERATED_BODY()
	
public:
	uint8 GetSelectedOptionIndex() const { return SelectedOptionIndex; }
	void SetSelectedOptionIndex(const uint8 NewIndex);

	uint8 GetDefaultOptionIndex() const { return DefaultOptionIndex; }
	void SetDefaultOptionIndex(const uint8 NewIndex);
	
	const TArray<FText>& GetOptionTexts() const { return OptionTexts; }
	void SetOptionTexts(const TArray<FText>& NewOptionsTexts);
	
	bool GetRequireConfirmation() const { return bRequireConfirmation; }
	void SetRequireConfirmation(const bool bNewRequireConfirmation);

	virtual bool RevertToDefaults() override;
	
private:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter, meta=(AllowPrivateAccess=true))
	uint8 SelectedOptionIndex = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	uint8 DefaultOptionIndex = 0;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	TArray<FText> OptionTexts;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter=GetRequireConfirmation, meta=(AllowPrivateAccess=true))
	bool bRequireConfirmation = false;
};
