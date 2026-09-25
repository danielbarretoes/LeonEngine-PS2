#include "GameFramework/PlayerController.h"

#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"

void APlayerController::Possess(ACharacter* Character)
{
	AController::Possess(Character);
}

void APlayerController::AddYawInput(float Val)
{
	FRotator Rotation = GetControlRotation();
	Rotation.Yaw += Val;
	SetControlRotation(Rotation);
}

void APlayerController::AddPitchInput(float Val)
{
	FRotator Rotation = GetControlRotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + Val, ViewPitchMin, ViewPitchMax);
	SetControlRotation(Rotation);
}

FVector APlayerController::TickInput(UGameEngine& /*engine*/)
{
	return {};
}

void APlayerController::UpdateCamera(UGameEngine& /*engine*/, float /*deltaTime*/)
{
}
