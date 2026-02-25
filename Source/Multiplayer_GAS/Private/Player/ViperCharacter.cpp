
#include "Player/ViperCharacter.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperAttributeSet.h"
#include "OnlineSessionSettings.h"

AViperCharacter::AViperCharacter():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &AViperCharacter::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &AViperCharacter::OnFindSessionComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &AViperCharacter::OnJoinSessionComplete))
{
	PrimaryActorTick.bCanEverTick = true;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ViperAbilitySystemComponent = CreateDefaultSubobject<UViperAbilitySystemComponent>("AbilitySystemComponent");
	ViperAbilitySystemComponent->SetIsReplicated(true);
	ViperAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	ViperAttributeSet = CreateDefaultSubobject<UViperAttributeSet>("Viper Attribute Set");
	
	if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld()))
	{
		OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Blue, FString::Printf(TEXT("Online Subsystem found with name %s"),*OnlineSubsystem->GetSubsystemName().ToString()));
	}
}

void AViperCharacter::ServerSideInit()
{
	ViperAbilitySystemComponent->InitAbilityActorInfo(this,this);
	ViperAbilitySystemComponent->ApplyInitialEffects();
}

void AViperCharacter::ClientSideInit()
{
	ViperAbilitySystemComponent->InitAbilityActorInfo(this,this);
}

void AViperCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AViperCharacter::CreateGameSession()
{
	if (!OnlineSessionInterface.IsValid()) return;
	
	if (OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnlineSessionInterface->DestroySession(NAME_GameSession);
	}
	OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	
	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->bIsLANMatch = false;
	SessionSettings->NumPublicConnections = 4;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bAllowJoinViaPresence = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bUsesPresence = true;
	SessionSettings->bUseLobbiesIfAvailable = true;
	SessionSettings->Set(FName("MatchType"),FString("FreeForAll"),EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(),NAME_GameSession,*SessionSettings);
}

void AViperCharacter::JoinGameSession()
{
	if (!OnlineSessionInterface.IsValid()) return;
	
	OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	// Find game sessions
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 10000;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
	
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),SessionSearch.ToSharedRef());
	
}

void AViperCharacter::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, FString::Printf(TEXT("Created session: %s"),*SessionName.ToString()));
		
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(FString("/Game/Maps/Lobby?listen"));
		}
	}
	else
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Failed to create session")));
	}
}

void AViperCharacter::OnFindSessionComplete(bool bWasSuccessful)
{
	if (!OnlineSessionInterface.IsValid()) return;
	
	for (FOnlineSessionSearchResult Result : SessionSearch->SearchResults)
	{
		FString ID = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		FString MatchType;
		Result.Session.SessionSettings.Get(FName("MatchType"),MatchType);
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, FString::Printf(TEXT("[ID: %s] [User: %s]"),*ID,*User));
		
		if (MatchType == FString("FreeForAll"))
		{
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, FString::Printf(TEXT("Joining match type: %s"),*MatchType));
			
			OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
			
			Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
			Result.Session.SessionSettings.bUsesPresence = true;
			
			const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(),NAME_GameSession,Result);
		}
	}
}

void AViperCharacter::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!OnlineSessionInterface.IsValid()) return;
	FString Address;
	
	if (OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, FString::Printf(TEXT("Connect string: %s"),*Address));
		
		if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PlayerController->ClientTravel(Address,ETravelType::TRAVEL_Absolute);
		}
	}
}

void AViperCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AViperCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

UViperAbilitySystemComponent* AViperCharacter::GetViperAbilitySystemComponent() const
{
	return Cast<UViperAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* AViperCharacter::GetAbilitySystemComponent() const
{
	return ViperAbilitySystemComponent;
}
