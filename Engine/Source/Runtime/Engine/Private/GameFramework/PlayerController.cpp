#include "GameFramework/PlayerController.h"

#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"

void APlayerController::Possess(ACharacter* Character)
{
	AController::Possess(Character);
}

glm::vec3 APlayerController::TickInput(UGameEngine& /*engine*/)
{
	return {};
}

void APlayerController::UpdateCamera(UGameEngine& /*engine*/, float /*deltaTime*/)
{
}
