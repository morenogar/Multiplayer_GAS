// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ViperSocialSubsystem.generated.h"

class UViperUserVM;
class UViperSquadVM;
class UViperSocialVM;
/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API UViperSocialSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintPure)
	UViperSocialVM* GetSocialViewModel();
	
private:
	UPROPERTY()
	TObjectPtr<UViperSocialVM> SocialViewModel;
	
	UFUNCTION()
	void PopulateSocialVM();
	UFUNCTION()
	void PopulateMySquad();
	
	//Squad
	void OnRemovedUserFromSquadChange(UObject* VMObject, UE::FieldNotification::FFieldId FieldID);
	void OnCreateSquadChange(UObject* VMObject, UE::FieldNotification::FFieldId FieldID);
	void OnSquadPrivacyChanged(UObject* VMObject, UE::FieldNotification::FFieldId FieldID);
	
	
	//Temporal for testing
	UViperSquadVM* RandomizeSquad();
	UViperUserVM* RandomizeUser();
	int32 UserID = 0;
};
