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

/** Input -> companion order (Companion.Order.Follow / MoveTo / Attack / UseAbility). */
USTRUCT(BlueprintType)
struct FViperCompanionOrderInput
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Companion.Order"))
	FGameplayTag OrderTag;
};

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

	/** Whatever the interact prompt is currently pointing at, or null. */
	UFUNCTION(BlueprintPure, Category = "Viper|Interaction")
	AActor* GetCurrentInteractable() const { return CurrentInteractable; }

	/** Interacts with it exactly as the input binding does. False when there is nothing in range. */
	UFUNCTION(BlueprintCallable, Category = "Viper|Interaction")
	bool TriggerInteract();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Input")
	UViperInputConfig* InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Abilities")
	UViperAbilitySet* AbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump")
	float JumpCooldown = 0.3f;

	/** Ground speed when not sprinting. This, and not the movement component, is the value to tune. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Movement", meta = (ClampMin = "0"))
	float WalkSpeed = 300.f;

	/** Ground speed while the sprint input is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viper|Movement", meta = (ClampMin = "0"))
	float SprintSpeed = 600.f;

	/** Replicated so simulated proxies and the anim graph can read it. */
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting, BlueprintReadOnly, Category = "Viper|Movement")
	bool bIsSprinting = false;

	UFUNCTION(BlueprintPure, Category = "Viper|Movement")
	bool IsSprinting() const { return bIsSprinting; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category="Interaction")
	float MaxInteractDistance = 500.f;

	UPROPERTY(EditAnywhere, Category="Interaction")
	float MinDotThreshold = 0.5f;
	
	/** Inputs that give orders to the recruited companion. The order uses what the camera is aiming at. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion")
	TArray<FViperCompanionOrderInput> CompanionOrderInputs;
	
	/** How far the aim trace for companion orders goes. */
	UPROPERTY(EditDefaultsOnly, Category = "Viper|Companion", meta = (ClampMin = "0"))
	float CompanionOrderTraceDistance = 5000.f;

protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	USphereComponent* InteractionSphere;
	
	bool bCanJumpAgain = true;
	FTimerHandle JumpCooldownHandle;
	
	
	UFUNCTION()
	void ResetJump();

	void InputAbilityInputTagPressed(FGameplayTag InputTag);
	void InputAbilityInputTagReleased(FGameplayTag InputTag);

	void HandleSprintStarted();
	void HandleSprintCompleted();

	/** Single place that owns MaxWalkSpeed, so walk and sprint never disagree. */
	void ApplyMovementSpeed();

	/*------------------------------- Weight ----------------------------------*/

	/**
	 * How fast the walk may change pace, in units per second squared. Breaking into a run is a
	 * change of gait, not a change of number: without this the speed doubles in the frame the key
	 * goes down, and with the engine template acceleration underneath it the character simply
	 * appears at the new speed. 0 switches instantly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Viper|Movement", meta = (ClampMin = "0"))
	float SpeedChangeRate = 900.f;

	/**
	 * Acceleration, braking and braking friction, set here rather than left at the engine template
	 * values (2048 / 2048 / 8). Those are tuned for an arcade feel and give a detective on a
	 * mountain path no weight at all: full speed and dead stop both arrive inside a tenth of a
	 * second. These are the numbers to reach for if the walk feels heavy or floaty.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Viper|Movement", meta = (ClampMin = "1"))
	float WalkAcceleration = 1400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Viper|Movement", meta = (ClampMin = "1"))
	float WalkBraking = 1100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Viper|Movement", meta = (ClampMin = "0"))
	float WalkBrakingFriction = 1.4f;

private:
	/** Rate-limited MaxWalkSpeed, so the gait changes over a few strides instead of in one frame. */
	float CurrentSpeed = 0.f;
	void SetSprinting(bool bNewSprinting);

	/** The owning client acts at once; the server has to agree or it will correct the move. */
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();
	
	/** Owning client: traces from the camera and sends the order to the server. */
	void HandleCompanionOrderInput(FGameplayTag OrderTag);
	
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
	UInputAction* SprintInputAction;
	
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
