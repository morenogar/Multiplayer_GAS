
#include "Player/ViperCharacter.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperAttributeSet.h"
#include "Interfaces/OnlineSessionInterface.h"

AViperCharacter::AViperCharacter()
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
