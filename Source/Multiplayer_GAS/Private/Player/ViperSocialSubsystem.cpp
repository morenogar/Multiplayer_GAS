// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperSocialSubsystem.h"
#include "Player/ViperSocialVM.h"

void UViperSocialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

UViperSocialVM* UViperSocialSubsystem::GetSocialViewModel()
{
	if (SocialViewModel != nullptr)
		return SocialViewModel;
 
	SocialViewModel = NewObject<UViperSocialVM>();
	
	return SocialViewModel;
}

void UViperSocialSubsystem::PopulateSocialVM()
{
	SocialViewModel->SetMyUserDetails(RandomizeUser());
	PopulateMySquad();
}

void UViperSocialSubsystem::PopulateMySquad()
{
	UViperSquadVM* Squad = RandomizeSquad();
	TArray<UViperUserVM*> Members = Squad->GetMembers();
	Members[1] = SocialViewModel->GetMyUserDetails();
	Squad->SetSquadLeader(Members[2]);
	Squad->SetMembers(Members);
	SocialViewModel->SetMySquad(Squad);
}

void UViperSocialSubsystem::OnRemovedUserFromSquadChange(UObject* VMObject, UE::FieldNotification::FFieldId FieldID)
{
	if (UViperUserVM* UserVM = Cast<UViperUserVM>(VMObject))
	{
		if(UserVM->GetRemoveFromSquad() == false) return;
		FString Message = FString::Printf(TEXT("Removed %s from your Squad"), *UserVM->GetUserName());
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
		
		//Here we should delete the user from our squad and bind an event when is donde 
	}
}

void UViperSocialSubsystem::OnCreateSquadChange(UObject* VMObject, UE::FieldNotification::FFieldId FieldID)
{
}

void UViperSocialSubsystem::OnSquadPrivacyChanged(UObject* VMObject, UE::FieldNotification::FFieldId FieldID)
{
	if (UViperSquadVM* SquadVM = Cast<UViperSquadVM>(VMObject))
	{
		//auto serverConnection = GetServerConnectionSystem();
		switch (SquadVM->GetSquadPrivacy())
		{
		case ESquadPrivacy::Public:
			//serverConnection->SetSquadPrivacy(ESesOnlineSquadPrivacyType::FriendsOnly);   CALL HERE THE FUNCTION THAT CHANGE THE PRIVACY IUN THE SERVER
			break;
		case ESquadPrivacy::Private:
			//serverConnection->SetSquadPrivacy(ESesOnlineSquadPrivacyType::Private);   CALL HERE THE FUNCTION THAT CHANGE THE PRIVACY IUN THE SERVER
			break;
		case ESquadPrivacy::MAX:
		case ESquadPrivacy::None:
		default:
			//serverConnection->SetSquadPrivacy(ESesOnlineSquadPrivacyType::Public);   CALL HERE THE FUNCTION THAT CHANGE THE PRIVACY IUN THE SERVER
			break;
		}
	}
}

UViperSquadVM* UViperSocialSubsystem::RandomizeSquad()
{
	static int32 SquadID = 0;
	SquadID ++;
	UViperSquadVM* Squad = NewObject<UViperSquadVM>();
	int32 RandomSquadMember = FMath::RandRange(3, 4);
	TArray<UViperUserVM*> SquadMembers;
    
	for (int i = 0; i < RandomSquadMember; ++i)
	{
		UViperUserVM* SquadMember = RandomizeUser();
		if(i == 0) Squad->SetSquadLeader(SquadMember);
		SquadMembers.Add(SquadMember);
	}
 
	Squad->SetMembers(SquadMembers);
	Squad->SetSquadID(FGuid (SquadID, 0, 0, 0));
 
	return Squad;
}

UViperUserVM* UViperSocialSubsystem::RandomizeUser()
{
	UViperUserVM* User = NewObject<UViperUserVM>();
    
    FString UserName = "User";
    UserName.AppendInt(UserID);
    User->SetUserName(UserName);
    
    UserID ++;
    User->SetUserID(FGuid (UserID, 0, 0, 0));
	
    User->SetRemoveFromSquad(false);
 

    User->AddFieldValueChangedDelegate(
    UViperUserVM::FFieldNotificationClassDescriptor::RemoveFromSquad, UE::FieldNotification::FFieldMulticastDelegate::FDelegate::CreateUObject(this, &UViperSocialSubsystem::OnRemovedUserFromSquadChange));
    
    return User;
}
