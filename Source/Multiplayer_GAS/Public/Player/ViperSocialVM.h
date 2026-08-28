// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ViperSocialVM.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class ESquadPrivacy : uint8
{
	None = 0,
	Public = 1,
	Private = 2,
	MAX UMETA(Hidden)
};

UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperUserVM : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:

	UViperUserVM(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer) 
	{
	}
	
	FString GetUserName() const { return UserName; }
	void SetUserName(const FString& NewUserName) { UE_MVVM_SET_PROPERTY_VALUE(UserName, NewUserName); }
	
	FString GetUserState() const { return UserState; }
	void SetUserState(const FString& NewUserState) { UE_MVVM_SET_PROPERTY_VALUE(UserState, NewUserState); }
	
	FGuid GetUserID() const { return UserID; }
	void SetUserID(const FGuid& NewUserID) { UE_MVVM_SET_PROPERTY_VALUE(UserID, NewUserID); }
	
	bool GetRemoveFromSquad() const { return bRemoveFromSquad; }
	void SetRemoveFromSquad(const bool& NewRemoveFromSquad) { UE_MVVM_SET_PROPERTY_VALUE(bRemoveFromSquad, NewRemoveFromSquad); }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	FString UserName;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, meta=(AllowPrivateAccess=true))
	FString UserState;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	FGuid UserID;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter=GetRemoveFromSquad, Setter=SetRemoveFromSquad, meta=(AllowPrivateAccess=true))
	bool bRemoveFromSquad;
};


UCLASS(BlueprintType)
class MULTIPLAYER_GAS_API UViperSquadVM : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	TArray<UViperUserVM*> GetMembers() const { return Members; }
	void SetMembers(const TArray<UViperUserVM*> NewMembers) { UE_MVVM_SET_PROPERTY_VALUE(Members, NewMembers); }
	
	UViperUserVM* GetSquadLeader() const { return SquadLeader; }
	void SetSquadLeader(UViperUserVM* NewSquadLeader) { UE_MVVM_SET_PROPERTY_VALUE(SquadLeader, NewSquadLeader); }
	
	ESquadPrivacy GetSquadPrivacy() const { return SquadPrivacy; }
	void SetSquadPrivacy(const ESquadPrivacy& NewSquadPrivacy) { UE_MVVM_SET_PROPERTY_VALUE(SquadPrivacy, NewSquadPrivacy); }
	
	FGuid GetSquadID() const { return SquadID; }
	void SetSquadID(const FGuid& NewSquadID) { UE_MVVM_SET_PROPERTY_VALUE(SquadID, NewSquadID); }

	bool GetCreateNewSquad() const { return bCreateNewSquad; }
	void SetCreateNewSquad(const bool& NewCreateNewSquad) { UE_MVVM_SET_PROPERTY_VALUE(bCreateNewSquad, NewCreateNewSquad); }

	
private:
	UPROPERTY(BlueprintReadWrite, FieldNotify,Getter, Setter, meta=(AllowPrivateAccess=true))
	TArray<UViperUserVM*> Members;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	UViperUserVM* SquadLeader;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter, meta=(AllowPrivateAccess=true))
	ESquadPrivacy SquadPrivacy = ESquadPrivacy::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	FGuid SquadID;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter=GetCreateNewSquad, Setter=SetCreateNewSquad, meta=(AllowPrivateAccess=true))
	bool bCreateNewSquad;
};


UCLASS()
class MULTIPLAYER_GAS_API UViperSocialVM : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	
	UViperUserVM* GetMyUserDetails() const { return MyUserDetails; }
	void SetMyUserDetails(UViperUserVM* NewMyUserDetails) { UE_MVVM_SET_PROPERTY_VALUE(MyUserDetails, NewMyUserDetails); };
	
	UViperSquadVM* GetMySquad() const { return MySquad; }
	void SetMySquad(UViperSquadVM* NewMySquad) {UE_MVVM_SET_PROPERTY_VALUE(MySquad, NewMySquad);};
	
private:
	
	UPROPERTY(BlueprintReadWrite, FieldNotify,Getter, Setter, meta=(AllowPrivateAccess=true))
	UViperSquadVM* MySquad;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, meta=(AllowPrivateAccess=true))
	UViperUserVM* MyUserDetails;
};
