#include "GameFramework/SpectatorPawn.h"

#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpectatorPawnMovement.h"

USpectatorPawnMovement::USpectatorPawnMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIgnoreTimeDilation = true;
}

ASpectatorPawn::ASpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USpectatorPawnMovement>(ADefaultPawn::MovementComponentName))
{
	// UE: a spectator is never hurt; its sphere collides with nothing (as Leon's default pawn's).
	bCanBeDamaged = false;
	BaseEyeHeight = 0.0f;
}

USpectatorPawnMovement* ASpectatorPawn::GetSpectatorPawnMovement() const
{
	return Cast<USpectatorPawnMovement>(GetMovementComponent());
}

void ASpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent != nullptr);
	if (!bAddDefaultMovementBindings)
	{
		return;
	}
	// ADefaultPawn's bindings (look first, then move), without its on-screen hint.
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ASpectatorPawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("TurnRate"), this, &ASpectatorPawn::TurnAtRate);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ASpectatorPawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis(TEXT("LookUpRate"), this, &ASpectatorPawn::LookUpAtRate);
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ASpectatorPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ASpectatorPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("MoveUp"), this, &ASpectatorPawn::MoveUp_World);
}
