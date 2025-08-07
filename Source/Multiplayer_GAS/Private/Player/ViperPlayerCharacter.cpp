// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
		EnhancedInputComponent->BindAction(JumpInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::Jump);
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::HandleLookInput);
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::HandleMoveInput);
	}
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
