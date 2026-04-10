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
#include "Interactables/ViperInteractableInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Interactables/ViperObjectDefinition.h"
#include "Interactables/ViperObjectDefinitionInterface.h"

AViperPlayerCharacter::AViperPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("Camera Boom");
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetupAttachment(GetRootComponent());

	ViewCamera = CreateDefaultSubobject<UCameraComponent>("View Camera");
	ViewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f,720.0f,0.0f);
	
	
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
	
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AViperPlayerCharacter::OnSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AViperPlayerCharacter::OnSphereEndOverlap);
}

void AViperPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SelectBestInteractable();
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

    FVector CamLoc;
    FRotator CamRot;
    GetController()->GetPlayerViewPoint(CamLoc, CamRot);

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
    	
    	// 1. Cast the Actor to the Interface pointer (prefixed with 'I')
    	IViperObjectDefinitionInterface* InterfaceInstance = Cast<IViperObjectDefinitionInterface>(CurrentInteractable);

    	if (InterfaceInstance)
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
    	
    	IViperObjectDefinitionInterface* InterfaceInstance = Cast<IViperObjectDefinitionInterface>(CurrentInteractable);

    	if (InterfaceInstance)
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
