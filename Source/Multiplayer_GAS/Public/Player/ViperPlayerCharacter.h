// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Player/ViperCharacter.h"
#include "ViperPlayerCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UCameraComponent;

/**
 * 
 */
UCLASS()
class MULTIPLAYER_GAS_API AViperPlayerCharacter : public AViperCharacter
{
	GENERATED_BODY()

public:
	AViperPlayerCharacter();

	virtual void PawnClientRestart() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleDefaultsOnly, Category= "View")
	USpringArmComponent* CameraBoom;
	
	UPROPERTY(VisibleDefaultsOnly, Category= "View")
	UCameraComponent* ViewCamera;

	UPROPERTY(EditDefaultsOnly, Category= "View")
	UInputAction* JumpInputAction;

	UPROPERTY(EditDefaultsOnly, Category= "View")
	UInputAction* LookInputAction;

	UPROPERTY(EditDefaultsOnly, Category= "View")
	UInputAction* MoveInputAction;
	
	UPROPERTY(EditDefaultsOnly, Category= "View")
	UInputMappingContext* GameplayInputMapping;

	UFUNCTION()
	void HandleLookInput(const FInputActionValue& InputActionValue);

	UFUNCTION()
    void HandleMoveInput(const FInputActionValue& InputActionValue);

	FVector GetLookRightDirection() const;
	FVector GetLookForwardDirection() const;
	FVector GetMoveForwardDirection() const;
};
