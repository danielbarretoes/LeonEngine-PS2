#include "GameFramework/PlayerController.h"

#include "Engine/GameEngine.h"
#include "Engine/Player.h"
#include "GameFramework/Character.h"

APlayerController::APlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
}

void APlayerController::Possess(ACharacter* Character)
{
	AController::Possess(Character);
}

void APlayerController::SetPlayer(UPlayer* InPlayer)
{
	check(InPlayer != nullptr);
	Player = InPlayer;
	InPlayer->PlayerController = this;
}

bool APlayerController::IsLocalController() const
{
	return Player != nullptr;
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
