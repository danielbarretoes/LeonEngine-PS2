#include <glm/geometric.hpp>

#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/Engine.h>
#include <leon/gameplay/DefaultCameraActor.h>
#include <leon/gameplay/DefaultPlayerController.h>

namespace leon {

DefaultCameraActor* DefaultPlayerController::GetDefaultCameraActor() const {
    return dynamic_cast<DefaultCameraActor*>(GetPawn());
}

glm::vec3 DefaultPlayerController::TickInput(Engine& engine) {
    DefaultCameraActor* cameraActor = GetDefaultCameraActor();
    if (cameraActor == nullptr) {
        return {};
    }

    const Camera& camera = engine.GetCamera();
    const float forwardAxis = engine.GetInput().GetAxisValue(InputActions::MoveForward);
    const float rightAxis = engine.GetInput().GetAxisValue(InputActions::MoveRight);
    const float upAxis = engine.GetInput().GetAxisValue(InputActions::MoveUp);

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

} // namespace leon
