
#include "Player/ViperCharacter.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperAttributeSet.h"

AViperCharacter::AViperCharacter()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ViperAbilitySystemComponent = CreateDefaultSubobject<UViperAbilitySystemComponent>("AbilitySystemComponent");
	ViperAbilitySystemComponent->SetIsReplicated(true);
	ViperAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	ViperAttributeSet = CreateDefaultSubobject<UViperAttributeSet>("Viper Attribute Set");
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
