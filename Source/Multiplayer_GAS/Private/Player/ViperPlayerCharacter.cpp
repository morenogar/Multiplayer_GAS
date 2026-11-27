// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ViperPlayerCharacter.h"

#include "AI/ViperCompanionStateTreeNodes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ViperAbilitySystemComponent.h"
#include "GAS/ViperBaseGameplayAbility.h"
#include "Input/ViperEnhancedInputComponent.h"
#include "Interactables/ViperInteractableInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Interactables/ViperObjectDefinition.h"
#include "Interactables/ViperObjectDefinitionInterface.h"
#include "AI/ViperCompanionCharacter.h"
#include "Player/ViperPlayerController.h"
#include "Net/UnrealNetwork.h"

AViperPlayerCharacter::AViperPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("Camera Boom");
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetupAttachment(GetRootComponent());

	ViewCamera = CreateDefaultSubobject<UCameraComponent>("View Camera");
	ViewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// The character faces where the camera faces, turning into it at RotationRate.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f,720.0f,0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// Weight. The engine template leaves acceleration and braking at 2048, where full speed and a
	// dead stop both arrive inside a tenth of a second: fine for an arcade character, wrong for a
	// detective walking a mountain path next to someone. All three are EditDefaultsOnly, so they
	// can be dialled in the Blueprint without coming back here.
	GetCharacterMovement()->MaxAcceleration = WalkAcceleration;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->BrakingFriction = WalkBrakingFriction;
	GetCharacterMovement()->BrakingDecelerationWalking = WalkBraking;
	
	
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetRootComponent());
	InteractionSphere->SetSphereRadius(300.f);

	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	InteractionSphere->SetGenerateOverlapEvents(true);

	CurrentInteractable = nullptr;
}

void AViperPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_Client)
	{
		// ability sets should be granted replicated already, try to activate the on spawn granted abilities
		for (auto AbilitySpec : ViperAbilitySystemComponent->GetActivatableAbilities())
		{
			UViperBaseGameplayAbility* Ability = Cast<UViperBaseGameplayAbility>(AbilitySpec.Ability);
			if (!Ability || Ability->GetActivationPolicy() != EViperAbilityActivationPolicy::OnSpawn)
			{
				continue;
			}
			Ability->TryActivateAbilityOnSpawn(ViperAbilitySystemComponent->AbilityActorInfo.Get(), AbilitySpec);
		}
	}
	
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AViperPlayerCharacter::OnSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AViperPlayerCharacter::OnSphereEndOverlap);

	// WalkSpeed owns the speed, even if the Blueprint brought its own MaxWalkSpeed.
	ApplyMovementSpeed();
}

void AViperPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Ease between the walk and the sprint instead of switching between them. ApplyMovementSpeed
	// owns which speed is wanted; this owns how fast the legs get there. It runs everywhere,
	// including on simulated proxies, because bIsSprinting is replicated - so the ramp other
	// players see is the same ramp the owner feels.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const float Target = bIsSprinting ? SprintSpeed : WalkSpeed;
		CurrentSpeed = SpeedChangeRate > 0.f
			? FMath::FInterpConstantTo(CurrentSpeed, Target, DeltaTime, SpeedChangeRate)
			: Target;
		Movement->MaxWalkSpeed = FMath::Max(1.f, CurrentSpeed);
	}

	SelectBestInteractable();
}

void AViperPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if(APlayerController* OwningPlayerController = GetController<APlayerController>())
	{
		// A player controller without a local player is a real thing - a remote client's proxy on
		// a listen server, and the test pilot - and GetLocalPlayer() is null for both. Calling
		// through it crashed the moment the pilot possessed a pawn.
		ULocalPlayer* LocalPlayer = OwningPlayerController->GetLocalPlayer();
		if(!LocalPlayer)
		{
			return;
		}

		if(UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
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
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Triggered, this, &AViperPlayerCharacter::HandleInteract);

		if (SprintInputAction)
		{
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Started, this, &AViperPlayerCharacter::HandleSprintStarted);
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Completed, this, &AViperPlayerCharacter::HandleSprintCompleted);
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Canceled, this, &AViperPlayerCharacter::HandleSprintCompleted);
		}
		
		if (AViperEIC && InputConfig)
		{
			TArray<uint32> BindHandles;
			AViperEIC->BindAbilityActions(InputConfig, this, &AViperPlayerCharacter::InputAbilityInputTagPressed, &AViperPlayerCharacter::InputAbilityInputTagReleased, BindHandles);
		}
		
		for (const FViperCompanionOrderInput& OrderInput : CompanionOrderInputs)
		{
			if (OrderInput.InputAction && OrderInput.OrderTag.IsValid())
			{
				EnhancedInputComponent->BindAction(OrderInput.InputAction.Get(), ETriggerEvent::Started, this, &AViperPlayerCharacter::HandleCompanionOrderInput, OrderInput.OrderTag);
			}
		}
	}
}

void AViperPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (GetNetMode() < NM_Client && ViperAbilitySystemComponent)
	{
		auto Lambda = [this]()
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
			for (auto AbilitySpec : ViperAbilitySystemComponent->GetActivatableAbilities())
			{
				UViperBaseGameplayAbility* Ability = Cast<UViperBaseGameplayAbility>(AbilitySpec.Ability);
				if (!Ability || Ability->GetActivationPolicy() != EViperAbilityActivationPolicy::OnSpawn)
				{
					continue;
				}
				Ability->TryActivateAbilityOnSpawn(ViperAbilitySystemComponent->AbilityActorInfo.Get(), AbilitySpec);
			}
		};

		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, Lambda), 1.0f, false);
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

void AViperPlayerCharacter::HandleCompanionOrderInput(FGameplayTag OrderTag)
{
	AViperPlayerController* ViperPlayerController = GetController<AViperPlayerController>();
	if (!ViperPlayerController || !ViperPlayerController->GetCompanion())
	{
		return;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	ViperPlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * CompanionOrderTraceDistance;
	
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CompanionOrderTrace), false, this);
	Params.AddIgnoredActor(ViperPlayerController->GetCompanion());
	
	// Object trace so character capsules (Pawn) are hit too: needed to aim Attack orders at characters.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, ViewLocation, TraceEnd, ObjectParams, Params);
	
	ViperPlayerController->ServerIssueCompanionOrder(OrderTag, bHit ? Hit.ImpactPoint : TraceEnd, bHit ? Hit.GetActor() : nullptr);
}

void AViperPlayerCharacter::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	if (OtherActor->GetClass()->ImplementsInterface(UViperObjectDefinitionInterface::StaticClass()))
	{
		InteractableActorsInRange.AddUnique(OtherActor);
	}
}

void AViperPlayerCharacter::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor) return;

	InteractableActorsInRange.Remove(OtherActor);

	if (OtherActor == CurrentInteractable)
	{
		CurrentInteractable = nullptr;
	}
}

void AViperPlayerCharacter::SelectBestInteractable()
{
	CurrentInteractable = nullptr;

	// Any controller, not only a player's: GetPlayerViewPoint is virtual on AController, and an AI
	// gets the pawn's eyes and its control rotation from it. Demanding a PlayerController here
	// meant an AI-driven pawn could never pick anything up - measured, the test pilot walked to
	// the companion, turned to face her, and still found "nothing in range" every time.
	if(AController* OwningController = GetController())
	{
		FVector CamLoc;
	    FRotator CamRot;
	    OwningController->GetPlayerViewPoint(CamLoc, CamRot);

	    FVector Forward = CamRot.Vector();
		
	    FHitResult Hit;
	    FVector TraceEnd = CamLoc + (Forward * MaxInteractDistance);

	    FCollisionQueryParams Params;
	    Params.AddIgnoredActor(this);

	    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params);
		
		if (bHit)
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Red, false, 0.f);

	    if (bHit && Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UViperObjectDefinitionInterface::StaticClass()))
	    {
	        CurrentInteractable = Hit.GetActor();

	        DrawDebugSphere(GetWorld(), CurrentInteractable->GetActorLocation(), 25.f, 12, FColor::Red, false, 0.1f);
    		
    		if (IViperObjectDefinitionInterface* InterfaceInstance = Cast<IViperObjectDefinitionInterface>(CurrentInteractable))
    		{
    			// 2. Call the function directly
    			if (const UViperObjectDefinition* ObjDef = InterfaceInstance->GetObjectDefinition())
    			{
    				BPE_InteractionUpdated(CurrentInteractable->GetActorLocation(), ObjDef);
    				
    				UE_LOG(LogTemp, Log, TEXT("Raycast hit: %s"), *ObjDef->GetDisplayName().ToString());
    				return;
    			}
    		}
	    }
		
	    if (InteractableActorsInRange.Num() == 0)
	    {
    		BPE_InteractionUpdated(FVector(0.0f,0.0f,0.0f), nullptr);
    		return;
	    }

	    float BestScore = -1.f;

		for (AActor* Actor : InteractableActorsInRange)
		{
			if (!Actor) continue;

			// 🔹 Dirección desde la cámara (lo que el jugador ve)
			FVector DirToActor = (Actor->GetActorLocation() - CamLoc).GetSafeNormal();
			float Dot = FVector::DotProduct(Forward, DirToActor);

			// 🔹 Filtrar por ángulo (cono de visión)
			if (Dot < MinDotThreshold)
				continue;

			// 🔹 Distancia desde el personaje (lo que puede alcanzar)
			float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());

			if (Distance > MaxInteractDistance)
				continue;

			// 🔹 Score combinado (prioriza centro de pantalla + cercanía)
			float Score = Dot - (Distance * 0.001f);

			if (Score > BestScore)
			{
				BestScore = Score;
				CurrentInteractable = Actor;
			}
		}

	    if (CurrentInteractable)
	    {
	        //FText Text = IViperInteractableInterface::Execute_GetInteractText(CurrentInteractable);

	        DrawDebugSphere(GetWorld(), CurrentInteractable->GetActorLocation(), 25.f, 12, FColor::Green, false, 0.1f);
    		
    		if (IViperObjectDefinitionInterface* InterfaceInstance = Cast<IViperObjectDefinitionInterface>(CurrentInteractable))
    		{
    			if (const UViperObjectDefinition* ObjDef = InterfaceInstance->GetObjectDefinition())
    			{
    				BPE_InteractionUpdated(CurrentInteractable->GetActorLocation(), ObjDef);
    				UE_LOG(LogTemp, Log, TEXT("Raycast hit: %s"), *ObjDef->GetDisplayName().ToString());
    			}
    			
    			return;
    		}
	    }
		
		BPE_InteractionUpdated(FVector(0.0f,0.0f,0.0f), nullptr);
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

void AViperPlayerCharacter::HandleInteract(const FInputActionValue& InputActionValue)
{
	TriggerInteract();
}

bool AViperPlayerCharacter::TriggerInteract()
{
	if (CurrentInteractable == nullptr)
	{
		// Which of the two ways of finding something came up empty, rather than just "nothing":
		// the trace needs line of sight to something with an object definition, the list needs
		// the interaction spheres to be overlapping.
		UE_LOG(LogViperCompanion, Verbose,
			TEXT("interact | nothing selected, %d actor(s) in range"), InteractableActorsInRange.Num());
		return false;
	}

	if (IViperInteractableInterface* InterfaceInstance = Cast<IViperInteractableInterface>(CurrentInteractable))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			InterfaceInstance->Interact(PlayerController);
			return true;
		}
	}

	return false;
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
	
	if(Controller != nullptr && InputValue.Y != 0.0)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, InputValue.Y);
	}
	
	if(Controller != nullptr && InputValue.X != 0.0)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, InputValue.X);
	}
}

FVector AViperPlayerCharacter::GetLookRightDirection() const
{
	return ViewCamera->GetRightVector();
}

void AViperPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The owner already set it locally when it pressed the key, so it does not need the echo.
	DOREPLIFETIME_CONDITION(AViperPlayerCharacter, bIsSprinting, COND_SkipOwner);
}

void AViperPlayerCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const float Target = bIsSprinting ? SprintSpeed : WalkSpeed;

	// First time through, or no ramp asked for: take the speed at once. Otherwise Tick walks it
	// there, so that breaking into a run is a change of gait over a few strides rather than a new
	// number arriving in one frame.
	if (CurrentSpeed <= 0.f || SpeedChangeRate <= 0.f)
	{
		CurrentSpeed = Target;
	}

	Movement->MaxWalkSpeed = FMath::Max(1.f, CurrentSpeed);
}

void AViperPlayerCharacter::SetSprinting(const bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;
	ApplyMovementSpeed();

	// Without this the server keeps simulating at the old speed and corrects the client back.
	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void AViperPlayerCharacter::ServerSetSprinting_Implementation(const bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	ApplyMovementSpeed();
}

void AViperPlayerCharacter::OnRep_IsSprinting()
{
	// Simulated proxies: keep the movement component in step with what they are shown.
	ApplyMovementSpeed();
}

void AViperPlayerCharacter::HandleSprintStarted()
{
	SetSprinting(true);
}

void AViperPlayerCharacter::HandleSprintCompleted()
{
	SetSprinting(false);
}

