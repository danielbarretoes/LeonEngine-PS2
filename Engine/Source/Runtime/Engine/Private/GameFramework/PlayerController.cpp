#include "GameFramework/PlayerController.h"

#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"

void APlayerController::Possess(ACharacter* Character)
{
	AController::Possess(Character);
}

FVector APlayerController::TickInput(UGameEngine& /*engine*/)
{
	return {};
}

void APlayerController::UpdateCamera(UGameEngine& /*engine*/, float /*deltaTime*/)
{
}
