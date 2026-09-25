#include "GameFramework/DefaultPawn.h"

#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/FloatingPawnMovement.h"

const FName ADefaultPawn::MovementComponentName(TEXT("MovementComponent0"));
const FName ADefaultPawn::CollisionComponentName(TEXT("CollisionComponent0"));

ADefaultPawn::ADefaultPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
	, bAddDefaultMovementBindings(true)
{
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(CollisionComponentName);
	CollisionComponent->InitSphereRadius(35.0f);
	RootComponent = CollisionComponent;

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(MovementComponentName);
	MovementComponent->UpdatedComponent = CollisionComponent;
	// The legacy fly camera's speed.
	MovementComponent->MaxSpeed = 800.0f;

	// The view is the actor location (UE: BaseEyeHeight 0 for the default pawn).
	BaseEyeHeight = 0.0f;
}

UPawnMovementComponent* ADefaultPawn::GetMovementComponent() const
{
	return MovementComponent;
}

void ADefaultPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent != nullptr);
	if (bAddDefaultMovementBindings)
	{
		// Look first, then move: a frame moves along the view the mouse just turned (the legacy fly camera's order).
		PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ADefaultPawn::AddControllerYawInput);
		PlayerInputComponent->BindAxis(TEXT("TurnRate"), this, &ADefaultPawn::TurnAtRate);
		PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ADefaultPawn::AddControllerPitchInput);
		PlayerInputComponent->BindAxis(TEXT("LookUpRate"), this, &ADefaultPawn::LookUpAtRate);
		PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ADefaultPawn::MoveForward);
		PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ADefaultPawn::MoveRight);
		PlayerInputComponent->BindAxis(TEXT("MoveUp"), this, &ADefaultPawn::MoveUp_World);
	}
	// Leon: the hint the legacy fly camera showed (the frames are compared with it on screen).
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FLinearColor(0.35f, 0.95f, 0.55f),
			TEXT("DefaultCameraActor -- mouse look, WASD fly, Q/E up/down"));
	}
}

void ADefaultPawn::MoveForward(float Val)
{
	if (Val != 0.0f && GetController() != nullptr)
	{
		// The view's forward, pitch included (the vector the legacy camera flew along).
		AddMovementInput(GetController()->GetControlRotation().Vector(), Val);
	}
}

void ADefaultPawn::MoveRight(float Val)
{
	if (Val != 0.0f && GetController() != nullptr)
	{
		AddMovementInput(FRotationMatrix(GetController()->GetControlRotation()).GetUnitAxis(EAxis::Y), Val);
	}
}

void ADefaultPawn::MoveUp_World(float Val)
{
	if (Val != 0.0f)
	{
		AddMovementInput(FVector::UpVector, Val);
	}
}

void ADefaultPawn::TurnAtRate(float Rate)
{
	if (Rate != 0.0f)
	{
		AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
	}
}

void ADefaultPawn::LookUpAtRate(float Rate)
{
	if (Rate != 0.0f)
	{
		AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
	}
}
