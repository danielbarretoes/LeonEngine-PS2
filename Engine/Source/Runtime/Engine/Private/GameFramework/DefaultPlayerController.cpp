#include "GameFramework/DefaultPlayerController.h"

#include "Engine/GameEngine.h"
#include "GameFramework/DefaultCameraActor.h"
#include "GameFramework/Input.h"
#include "GameFramework/InputActions.h"

#include <glm/geometric.hpp>

ADefaultCameraActor* ADefaultPlayerController::GetDefaultCameraActor() const
{
	return dynamic_cast<ADefaultCameraActor*>(GetPawn());
}

glm::vec3 ADefaultPlayerController::TickInput(UGameEngine& Engine)
{
	ADefaultCameraActor* CameraActor = GetDefaultCameraActor();
	if (CameraActor == nullptr)
	{
		return {};
	}

	const UCameraComponent& Camera = Engine.GetCamera();
	const float ForwardAxis = Engine.GetInput().GetAxisValue(Leon::InputActions::MoveForward);
	const float RightAxis = Engine.GetInput().GetAxisValue(Leon::InputActions::MoveRight);
	const float UpAxis = Engine.GetInput().GetAxisValue(Leon::InputActions::MoveUp);

	glm::vec3 Wish = (Camera.ForwardVector() * ForwardAxis) + (Camera.RightVector() * RightAxis) +
		(glm::vec3{0.0f, 1.0f, 0.0f} * UpAxis);

	const float Len = glm::length(Wish);
	if (Len > 1.0e-4f)
	{
		Wish /= Len;
	}
	else
	{
		Wish = {};
	}
	return Wish;
}
