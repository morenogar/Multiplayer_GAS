// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ViperGameSettingsSubsystem.generated.h"

/**
 * 
 */

class UViperGameSettingsVM;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSettingUpdated, const FGameplayTag&, FieldId);

USTRUCT(BlueprintType)
struct FBasicSettingInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Id = FGameplayTag::EmptyTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Name = FText::GetEmpty();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description = FText::GetEmpty();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag CategoryID = FGameplayTag::EmptyTag;

	FBasicSettingInfo()
	{}

	FBasicSettingInfo(const FGameplayTag& InId,
		FText InName,
		FText InDescription,
		FGameplayTag InCategoryID)
		: Id(InId)
		, Name(InName)
		, Description(InDescription)
		, CategoryID(InCategoryID)
	{}
};

USTRUCT(BlueprintType)
struct FSliderSettingInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentValue = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultValue = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinValue = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxValue = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StepSize = 0.1f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StepTime = 0.1f;
};

USTRUCT(BlueprintType)
struct FMultiOptionSettingInfo
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 CurrentOptionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 DefaultOptionIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FText> OptionTexts = TArray<FText>();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDoesOptionRequireConfirmation = false;
};

UCLASS()
class MULTIPLAYER_GAS_API UViperGameSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintPure)
	UViperGameSettingsVM* GetGameSettingsViewModel();
	
	void PopulateSettingsVM(UViperGameSettingsVM& SettingsVM);
	void PopulateInputSettingsGeneral(UViperGameSettingsVM& SettingsVM);
	void PopulateAudioSettings(UViperGameSettingsVM& SettingsVM);
	void PopulateVisualsSettings(UViperGameSettingsVM& SettingsVM);

	void CreateAndRegisterSettingSlider(FBasicSettingInfo SettingInfo, FSliderSettingInfo SliderSettingInfo);
	void CreateAndRegisterSettingMultiOption(FBasicSettingInfo SettingInfo, FMultiOptionSettingInfo MultiOptionSettingInfo);

	void OnSettingSliderChanged(UObject* VMObject, UE::FieldNotification::FFieldId FieldId);
	void OnSettingMultiOptionChanged(UObject* VMObject, UE::FieldNotification::FFieldId FieldId);
	
	UPROPERTY(BlueprintAssignable)
	FSettingUpdated OnSettingUpdated;
	
	UPROPERTY()
	TObjectPtr<UViperGameSettingsVM> GameSettingsVM;
};
