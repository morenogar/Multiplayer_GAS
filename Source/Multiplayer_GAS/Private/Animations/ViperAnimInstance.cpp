// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/ViperAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"


void UViperAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	
	if(OwnerCharacter)
	{
		OwnerMovementComp = OwnerCharacter->GetCharacterMovement();
	}
}

void UViperAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if(OwnerCharacter)
	{
		Speed = OwnerCharacter->GetVelocity().Length();
		FRotator BodyRotation = OwnerCharacter->GetActorRotation();
		FRotator BodyRotationDelta = UKismetMathLibrary::NormalizedDeltaRotator(BodyRotation,BodyPreviousRotation);
		BodyPreviousRotation = BodyRotation;

		YawSpeed = BodyRotationDelta.Yaw / DeltaSeconds;
		SmoothedYawSpeed = UKismetMathLibrary::FInterpTo(SmoothedYawSpeed,YawSpeed,DeltaSeconds,YawSpeedSmoothLerpSpeed);

		FRotator ControlRotation = OwnerCharacter->GetBaseAimRotation();
		LookRotationOffset = UKismetMathLibrary::NormalizedDeltaRotator(ControlRotation,BodyRotation);
	}

	if(OwnerMovementComp)
	{
		bIsJumping = OwnerMovementComp->IsFalling();
	}
}

void UViperAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
}
