#include "CoopTpPlayerController.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>

#include "CoopTpCharacter.h"

namespace game {

CoopTpCharacter* CoopTpPlayerController::GetCoopCharacter() const {
    return dynamic_cast<CoopTpCharacter*>(GetCharacter());
}

glm::vec3 CoopTpPlayerController::TickInput(leon::Engine& engine) {
    CoopTpCharacter* character = GetCoopCharacter();
    if (character == nullptr) {
        mouseLookSampleValid_ = false;
        return {};
    }

    leon::SpringArmComponent& boom = character->SpringArm();

    if (!IsLocalController()) {
        if (!hasPendingRemote_ && !pendingRemoteJumpLatch_) {
            return {};
        }
        hasPendingRemote_ = false;
        boom.BoomYawDegrees = pendingRemote_.lookYaw;
        boom.BoomPitchDegrees = pendingRemote_.lookPitch;
        boom.ClampPitch();

        const glm::vec3 move{pendingRemote_.moveX, 0.0f, pendingRemote_.moveZ};
        character->AddMovementInput(move);
        const bool jump = pendingRemote_.jump != 0 || pendingRemoteJumpLatch_;
        pendingRemote_.jump = 0;
        pendingRemoteJumpLatch_ = false;
        if (jump) {
            character->Jump();
        }
        return move;
    }

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

    const bool jump = engine.GetInput().WasActionJustPressed(leon::InputActions::Jump);
    if (jump) {
        character->Jump();
        localJumpRepeatFrames_ = 3;
    }

    lastLocalCmd_.type = static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd);
    lastLocalCmd_.seq = ++localSeq_;
    lastLocalCmd_.moveX = move.x;
    lastLocalCmd_.moveZ = move.z;
    lastLocalCmd_.lookYaw = boom.BoomYawDegrees;
    lastLocalCmd_.lookPitch = boom.BoomPitchDegrees;
    if (localJumpRepeatFrames_ > 0) {
        lastLocalCmd_.jump = 1;
        --localJumpRepeatFrames_;
    } else {
        lastLocalCmd_.jump = 0;
    }

    return move;
}

void CoopTpPlayerController::UpdateCamera(leon::Engine& engine, float deltaTime) {
    CoopTpCharacter* character = GetCoopCharacter();
    if (character == nullptr || !IsLocalController()) {
        return;
    }
    character->SpringArm().ApplyToCamera(engine.GetCamera(), character->GetActorLocation(),
                                         deltaTime);
}

void CoopTpPlayerController::ApplyRemoteInput(const leon::net::InputCmdMsg& cmd) {
    if (cmd.jump != 0) {
        pendingRemoteJumpLatch_ = true;
    }
    pendingRemote_ = cmd;
    hasPendingRemote_ = true;
}

leon::net::InputCmdMsg CoopTpPlayerController::ConsumeLocalInputCmd() {
    return lastLocalCmd_;
}

leon::net::InputCmdMsg CoopTpPlayerController::MakeIdleInputCmd() {
    lastLocalCmd_.type = static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd);
    lastLocalCmd_.seq = ++localSeq_;
    lastLocalCmd_.moveX = 0.0f;
    lastLocalCmd_.moveZ = 0.0f;
    lastLocalCmd_.jump = 0;
    localJumpRepeatFrames_ = 0;
    return lastLocalCmd_;
}

} // namespace game
