#include "ThirdPersonPlayerController.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>

#include "ThirdPersonCharacter.h"

namespace game {

ThirdPersonCharacter* ThirdPersonPlayerController::GetThirdPersonCharacter() const {
    return dynamic_cast<ThirdPersonCharacter*>(GetCharacter());
}

glm::vec3 ThirdPersonPlayerController::TickInput(leon::Engine& engine) {
    ThirdPersonCharacter* character = GetThirdPersonCharacter();
    if (character == nullptr) {
        mouseLookSampleValid_ = false;
        return {};
    }

    leon::SpringArmComponent& boom = character->SpringArm();
    leon::Window& inputWindow = engine.GetPlayInputWindow();

    double mouseX = 0.0;
    double mouseY = 0.0;
    inputWindow.GetCursorPos(mouseX, mouseY);

    const bool wantLook = !engine.IsCameraDragSuppressed() &&
                          (engine.IsCursorCaptured() || engine.IsPlayMouseLookActive() ||
                           inputWindow.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT));

    if (wantLook) {
        if (mouseLookSampleValid_) {
            const float dx = static_cast<float>(mouseX - lastMouseX_);
            const float dy = static_cast<float>(mouseY - lastMouseY_);
            constexpr float kLookDegreesPerPixel = 0.25f;
            boom.AddYawInput(dx * kLookDegreesPerPixel);
            boom.AddPitchInput(dy * kLookDegreesPerPixel);
        }
        mouseLookSampleValid_ = true;
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
    } else {
        mouseLookSampleValid_ = false;
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
    }

    const float scrollY = engine.ConsumeScrollY();
    if (scrollY != 0.0f) {
        boom.AddArmLengthInput(-scrollY * 0.4f);
    }

    const leon::MoveAxes2D axes = engine.GetInput().GetMoveAxes2D();
    const glm::vec3 move = boom.GetMoveDirectionXZ(axes);
    character->AddMovementInput(move);

    if (engine.GetInput().WasActionJustPressed(leon::InputActions::Jump)) {
        character->Jump();
    }

    return move;
}

void ThirdPersonPlayerController::UpdateCamera(leon::Engine& engine, float deltaTime) {
    ThirdPersonCharacter* character = GetThirdPersonCharacter();
    if (character == nullptr) {
        return;
    }
    character->SpringArm().ApplyToCamera(engine.GetCamera(), character->GetActorLocation(),
                                         deltaTime);
}

} // namespace game
