#include "FurytoonPlayerController.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>

#include "FurytoonCharacter.h"

namespace game {

FurytoonCharacter* FurytoonPlayerController::GetFurytoonCharacter() const {
    return dynamic_cast<FurytoonCharacter*>(GetCharacter());
}

bool FurytoonPlayerController::ConsumeLightAttack() {
    if (pendingLightLatch_) {
        pendingLightLatch_ = false;
        return true;
    }
    if (!lightRequested_) {
        return false;
    }
    lightRequested_ = false;
    return true;
}

bool FurytoonPlayerController::ConsumeHeavyAttack() {
    if (pendingHeavyLatch_) {
        pendingHeavyLatch_ = false;
        return true;
    }
    if (!heavyRequested_) {
        return false;
    }
    heavyRequested_ = false;
    return true;
}

void FurytoonPlayerController::ClearCombatInput() {
    lightRequested_ = false;
    heavyRequested_ = false;
    pendingLightLatch_ = false;
    pendingHeavyLatch_ = false;
    pendingRemoteJumpLatch_ = false;
    hasPendingRemote_ = false;
    localJumpRepeatFrames_ = 0;
    lastLocalCmd_.buttons = 0;
    lastLocalCmd_.jump = 0;
    lastLocalCmd_.moveX = 0.0f;
    lastLocalCmd_.moveZ = 0.0f;
    LatchButtons(0);
}

glm::vec3 FurytoonPlayerController::TickInput(leon::Engine& engine) {
    FurytoonCharacter* character = GetFurytoonCharacter();
    if (character == nullptr) {
        return {};
    }

    leon::SpringArmComponent& boom = character->SpringArm();

    if (!IsLocalController()) {
        if (hasPendingRemote_ || pendingRemoteJumpLatch_) {
            hasPendingRemote_ = false;
            // Keep boom yaw aligned with shared arena camera for remote move basis.
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
        return {};
    }

    // Shared top-down camera: WASD relative to arena camera yaw (no mouse look).
    const float camYaw = engine.GetCamera().YawDegrees();
    boom.BoomYawDegrees = camYaw;
    boom.BoomPitchDegrees = engine.GetCamera().PitchDegrees();

    const leon::MoveAxes2D axes = engine.GetInput().GetMoveAxes2D();
    const glm::vec3 move = leon::yawRelativeMoveXZ(camYaw, axes);
    character->AddMovementInput(move);

    const bool jump = engine.GetInput().WasActionJustPressed(leon::InputActions::Jump);
    if (jump) {
        character->Jump();
        localJumpRepeatFrames_ = 3;
    }

    leon::Window& inputWindow = engine.GetPlayInputWindow();
    const bool lightDown = inputWindow.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    const bool heavyDown = inputWindow.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
    std::uint16_t held = 0;
    if (lightDown) {
        held = static_cast<std::uint16_t>(held | leon::net::InputButtons::Primary);
    }
    if (heavyDown) {
        held = static_cast<std::uint16_t>(held | leon::net::InputButtons::Secondary);
    }
    LatchButtons(held);
    lightRequested_ = WasButtonPressed(leon::net::InputButtons::Primary);
    heavyRequested_ = WasButtonPressed(leon::net::InputButtons::Secondary);

    lastLocalCmd_.type = static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd);
    lastLocalCmd_.seq = ++localSeq_;
    lastLocalCmd_.moveX = move.x;
    lastLocalCmd_.moveZ = move.z;
    lastLocalCmd_.lookYaw = camYaw;
    lastLocalCmd_.lookPitch = boom.BoomPitchDegrees;
    lastLocalCmd_.jump = localJumpRepeatFrames_ > 0 ? 1 : 0;
    if (localJumpRepeatFrames_ > 0) {
        --localJumpRepeatFrames_;
    }
    leon::net::SetInputButton(lastLocalCmd_, leon::net::InputButtons::Primary, lightRequested_);
    leon::net::SetInputButton(lastLocalCmd_, leon::net::InputButtons::Secondary, heavyRequested_);

    return move;
}

void FurytoonPlayerController::UpdateCamera(leon::Engine& /*engine*/, float /*deltaTime*/) {
    // Shared arena camera is owned by FurytoonGameMode::UpdateSharedArenaCamera.
}

void FurytoonPlayerController::ApplyRemoteInput(const leon::net::InputCmdMsg& cmd) {
    if (cmd.jump != 0) {
        pendingRemoteJumpLatch_ = true;
    }
    if (leon::net::HasInputButton(cmd, leon::net::InputButtons::Primary)) {
        pendingLightLatch_ = true;
    }
    if (leon::net::HasInputButton(cmd, leon::net::InputButtons::Secondary)) {
        pendingHeavyLatch_ = true;
    }
    pendingRemote_ = cmd;
    hasPendingRemote_ = true;
}

leon::net::InputCmdMsg FurytoonPlayerController::ConsumeLocalInputCmd() {
    return lastLocalCmd_;
}

leon::net::InputCmdMsg FurytoonPlayerController::MakeIdleInputCmd() {
    ClearCombatInput();
    lastLocalCmd_.type = static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd);
    lastLocalCmd_.seq = ++localSeq_;
    return lastLocalCmd_;
}

} // namespace game
