#include "GameFramework/Pawn.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

APawn::APawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FRotator APawn::GetControlRotation() const
{
	return Controller != nullptr ? Controller->GetControlRotation() : FRotator::ZeroRotator;
}

FRotator APawn::GetViewRotation() const
{
	return Controller != nullptr ? Controller->GetControlRotation() : GetActorRotation();
}

void APawn::AddControllerYawInput(float Val)
{
	if (Val == 0.0f)
	{
		return;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->AddYawInput(Val);
	}
}

void APawn::AddControllerPitchInput(float Val)
{
	if (Val == 0.0f)
	{
		return;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->AddPitchInput(Val);
	}
}

void APawn::DetachController()
{
	if (Controller == nullptr)
	{
		return;
	}
	// Controller::UnPossess clears Pawn and calls bindController(nullptr).
	Controller->UnPossess();
}

void APawn::Destroyed()
{
	DetachController();
	Super::Destroyed();
}

void APawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachController();
	Super::EndPlay(EndPlayReason);
}
