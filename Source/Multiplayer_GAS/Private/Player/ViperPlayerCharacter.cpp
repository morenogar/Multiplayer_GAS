// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperBaseGameplayAbility.h"
#include "Input/ViperEnhancedInputComponent.h"

AViperPlayerCharacter::AViperPlayerCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("Camera Boom");
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetupAttachment(GetRootComponent());

	ViewCamera = CreateDefaultSubobject<UCameraComponent>("View Camera");
	ViewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f,720.0f,0.0f);
}

void AViperPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_Client)
	{
		// ability sets should be granted replicated already, try to activate the on spawn granted abilities
		for (auto abilitySpec : ViperAbilitySystemComponent->GetActivatableAbilities())
		{
			UViperBaseGameplayAbility* ability = Cast<UViperBaseGameplayAbility>(abilitySpec.Ability);
			if (!ability || ability->GetActivationPolicy() != EViperAbilityActivationPolicy::OnSpawn)
			{
				continue;
			}
			ability->TryActivateAbilityOnSpawn(ViperAbilitySystemComponent->AbilityActorInfo.Get(), abilitySpec);
		}
	}
}

void AViperPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if(APlayerController* OwningPlayerController = GetController<APlayerController>())
	{
		if(UEnhancedInputLocalPlayerSubsystem* InputSubsystem = OwningPlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->RemoveMappingContext(GameplayInputMapping);
			InputSubsystem->AddMappingContext(GameplayInputMapping,0);
		}
		
	}
}

void AViperPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if(UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UViperEnhancedInputComponent* AViperEIC = Cast<UViperEnhancedInputComponent>(EnhancedInputComponent);

		EnhancedInputComponent->BindAction(JumpInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::Jump);
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::HandleLookInput);
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::HandleMoveInput);
		
		if (AViperEIC && InputConfig)
		{
			TArray<uint32> BindHandles;
			AViperEIC->BindAbilityActions(InputConfig, this, &AViperPlayerCharacter::InputAbilityInputTagPressed, &AViperPlayerCharacter::InputAbilityInputTagReleased, BindHandles);
		}
	}
}

void AViperPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (GetNetMode() < NM_Client && ViperAbilitySystemComponent)
	{
		auto lambda = [this]()
		{
			if (!IsValid(this))
			{
				return;
			}
			if (this->IsPendingKillPending())
			{
				return;
			}
			if (!IsValid(ViperAbilitySystemComponent))
			{
				return;
			}
			
			ViperAbilitySystemComponent->InitAbilityActorInfo(this, this);
			// only grant abilities on authoritative systems
			ViperAbilitySystemComponent->GrantDefaultAbilities(AbilitySets);
			// can try to activate any server on spawn abilities immediately
			for (auto abilitySpec : ViperAbilitySystemComponent->GetActivatableAbilities())
			{
				UViperBaseGameplayAbility* ability = Cast<UViperBaseGameplayAbility>(abilitySpec.Ability);
				if (!ability || ability->GetActivationPolicy() != EViperAbilityActivationPolicy::OnSpawn)
				{
					continue;
				}
				ability->TryActivateAbilityOnSpawn(ViperAbilitySystemComponent->AbilityActorInfo.Get(), abilitySpec);
			}
		};
		
		FTimerHandle handle;
		GetWorldTimerManager().SetTimer(handle, FTimerDelegate::CreateWeakLambda(this, lambda), 1.0f, false);
	}

	SetOwner(NewController);
}

void AViperPlayerCharacter::ResetJump()
{
	GetWorldTimerManager().ClearTimer(JumpCooldownHandle);
	bCanJumpAgain = true;
}

void AViperPlayerCharacter::InputAbilityInputTagPressed(FGameplayTag InputTag)
{
	if (ViperAbilitySystemComponent)
	{
		ViperAbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AViperPlayerCharacter::InputAbilityInputTagReleased(FGameplayTag InputTag)
{
	if (ViperAbilitySystemComponent)
	{
		ViperAbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

void AViperPlayerCharacter::Jump()
{
	if (bCanJumpAgain)
	{
		Super::Jump();
		bCanJumpAgain = false;
	}
}

void AViperPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	GetWorldTimerManager().SetTimer(JumpCooldownHandle, this, &AViperPlayerCharacter::ResetJump, JumpCooldown, false);
}


void AViperPlayerCharacter::HandleLookInput(const FInputActionValue& InputActionValue)
{
	FVector2d InputValue  = InputActionValue.Get<FVector2d>();

	AddControllerPitchInput(InputValue.Y * -1);
	AddControllerYawInput(InputValue.X);
	
}

void AViperPlayerCharacter::HandleMoveInput(const FInputActionValue& InputActionValue)
{
	FVector2d InputValue  = InputActionValue.Get<FVector2d>();
	InputValue.Normalize();

	AddMovementInput(GetMoveForwardDirection() * InputValue.Y + GetLookRightDirection() * InputValue.X);
}

FVector AViperPlayerCharacter::GetLookRightDirection() const
{
	return ViewCamera->GetRightVector();
}

FVector AViperPlayerCharacter::GetLookForwardDirection() const
{
	return ViewCamera->GetForwardVector();
}

FVector AViperPlayerCharacter::GetMoveForwardDirection() const
{
	return FVector::CrossProduct(GetLookRightDirection(),FVector::UpVector);
}
