// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Player/ViperCharacter.h"
#include "ViperPlayerCharacter.generated.h"

class UViperObjectDefinition;
class USphereComponent;
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
	
	virtual void Tick(float DeltaTime) override;

	virtual void PawnClientRestart() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PossessedBy(AController* NewController) override;
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Viper|Interaction")
	void BPE_InteractionUpdated(FVector Location, const UViperObjectDefinition* ObjectDefinition);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Input")
	UViperInputConfig* InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Abilities")
	UViperAbilitySet* AbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump")
	float JumpCooldown = 0.3f;
	
	UPROPERTY(EditAnywhere, Category="Interaction")
	float MaxInteractDistance = 500.f;

	UPROPERTY(EditAnywhere, Category="Interaction")
	float MinDotThreshold = 0.5f;

protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	USphereComponent* InteractionSphere;
	
	bool bCanJumpAgain = true;
	FTimerHandle JumpCooldownHandle;
	
	
	UFUNCTION()
	void ResetJump();

	void InputAbilityInputTagPressed(FGameplayTag InputTag);
	void InputAbilityInputTagReleased(FGameplayTag InputTag);
	
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,int32 OtherBodyIndex);

	void UpdateInteractablesInRange();
	void SelectBestInteractable();

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
	UInputAction* InteractInputAction;
	
	UPROPERTY(EditDefaultsOnly, Category= "View")
	UInputMappingContext* GameplayInputMapping;
	
	UPROPERTY()
	TArray<AActor*> InteractableActorsInRange;
	
	UPROPERTY()
	AActor* CurrentInteractable;
	
	UFUNCTION()
	void HandleInteract(const FInputActionValue& InputActionValue);
	
	UFUNCTION()
	void HandleLookInput(const FInputActionValue& InputActionValue);
	
	UFUNCTION()
	void HandleMoveInput(const FInputActionValue& InputActionValue);

	FVector GetLookRightDirection() const;
};
