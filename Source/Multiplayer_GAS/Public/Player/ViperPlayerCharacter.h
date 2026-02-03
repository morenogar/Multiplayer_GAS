// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Player/ViperCharacter.h"
#include "ViperPlayerCharacter.generated.h"

class UViperAbilitySet;
class UViperInputConfig;
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

	virtual void BeginPlay() override;

	virtual void PawnClientRestart() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Input")
	UViperInputConfig* InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Abilities")
	UViperAbilitySet* AbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump")
	float JumpCooldown = 0.3f;


protected:

	bool bCanJumpAgain = true;
	FTimerHandle JumpCooldownHandle;

	UFUNCTION()
	void ResetJump();

	void InputAbilityInputTagPressed(FGameplayTag InputTag);
	void InputAbilityInputTagReleased(FGameplayTag InputTag);

	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;

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
