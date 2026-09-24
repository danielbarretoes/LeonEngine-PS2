#include <glm/geometric.hpp>

#include "GameFramework/Input.h"
#include "GameFramework/InputActions.h"
#include "Engine/GameEngine.h"
#include "GameFramework/DefaultCameraActor.h"
#include "GameFramework/DefaultPlayerController.h"


DefaultCameraActor* DefaultPlayerController::GetDefaultCameraActor() const {
    return dynamic_cast<DefaultCameraActor*>(GetPawn());
}

glm::vec3 DefaultPlayerController::TickInput(Engine& engine) {
    DefaultCameraActor* cameraActor = GetDefaultCameraActor();
    if (cameraActor == nullptr) {
        return {};
    }

    const Camera& camera = engine.GetCamera();
    const float forwardAxis = engine.GetInput().GetAxisValue(Leon::InputActions::MoveForward);
    const float rightAxis = engine.GetInput().GetAxisValue(Leon::InputActions::MoveRight);
    const float upAxis = engine.GetInput().GetAxisValue(Leon::InputActions::MoveUp);

    glm::vec3 wish = (camera.ForwardVector() * forwardAxis) + (camera.RightVector() * rightAxis) +
                     (glm::vec3{0.0f, 1.0f, 0.0f} * upAxis);

    const float len = glm::length(wish);
    if (len > 1.0e-4f) {
        wish /= len;
    } else {
        wish = {};
    }
    return wish;
}

