#include "ZombiesPlayerController.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>

#include "ZombiesCharacter.h"

namespace game {

ZombiesCharacter* ZombiesPlayerController::GetZombiesCharacter() const {
    return dynamic_cast<ZombiesCharacter*>(GetCharacter());
}

bool ZombiesPlayerController::ConsumeReloadRequested() {
    if (pendingRemoteReloadLatch_) {
        pendingRemoteReloadLatch_ = false;
        return true;
    }
    if (!reloadRequested_) {
        return false;
    }
    reloadRequested_ = false;
    return true;
}

bool ZombiesPlayerController::ConsumeUseRequested() {
    if (pendingRemoteUseLatch_) {
        pendingRemoteUseLatch_ = false;
        return true;
    }
    if (!useRequested_) {
        return false;
    }
    useRequested_ = false;
    return true;
}

void ZombiesPlayerController::ClearCombatInput() {
    fireHeld_ = false;
    lastRemoteFireHeld_ = false;
    reloadRequested_ = false;
    reloadKeyWasDown_ = false;
    useRequested_ = false;
    useKeyWasDown_ = false;
    pendingRemoteReloadLatch_ = false;
    pendingRemoteUseLatch_ = false;
    pendingRemoteJumpLatch_ = false;
    hasPendingRemote_ = false;
    localJumpRepeatFrames_ = 0;
    lastLocalCmd_.buttons = 0;
    lastLocalCmd_.jump = 0;
    lastLocalCmd_.moveX = 0.0f;
    lastLocalCmd_.moveZ = 0.0f;
}

glm::vec3 ZombiesPlayerController::TickInput(leon::Engine& engine) {
    ZombiesCharacter* character = GetZombiesCharacter();
    if (character == nullptr) {
        mouseLookSampleValid_ = false;
        fireHeld_ = false;
        reloadRequested_ = false;
        return {};
    }

    leon::SpringArmComponent& boom = character->SpringArm();

    if (!IsLocalController()) {
        // Reload latch is consumed in GameMode::ProcessPlayerReloads — do not gate movement on it.
        if (hasPendingRemote_ || pendingRemoteJumpLatch_) {
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
            fireHeld_ = lastRemoteFireHeld_;
            return move;
        }
        fireHeld_ = lastRemoteFireHeld_;
        return {};
    }

    leon::Window& inputWindow = engine.GetPlayInputWindow();
    double mouseX = 0.0;
    double mouseY = 0.0;
    inputWindow.GetCursorPos(mouseX, mouseY);

    const bool wantLook = engine.IsCursorCaptured() && !engine.IsCameraDragSuppressed();
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
    boom.ClampPitch();

    const leon::MoveAxes2D axes = engine.GetInput().GetMoveAxes2D();
    const glm::vec3 move = boom.GetMoveDirectionXZ(axes);
    character->AddMovementInput(move);

    const bool jump = engine.GetInput().WasActionJustPressed(leon::InputActions::Jump);
    if (jump) {
        character->Jump();
        localJumpRepeatFrames_ = 3;
    }

    fireHeld_ = engine.IsCursorCaptured() && inputWindow.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    const bool rDown = engine.IsCursorCaptured() && inputWindow.IsKeyPressed(GLFW_KEY_R);
    reloadRequested_ = rDown && !reloadKeyWasDown_;
    reloadKeyWasDown_ = rDown;

    const bool fDown = engine.IsCursorCaptured() && inputWindow.IsKeyPressed(GLFW_KEY_F);
    useRequested_ = fDown && !useKeyWasDown_;
    useKeyWasDown_ = fDown;

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
    leon::net::SetInputButton(lastLocalCmd_, leon::net::InputButtons::Fire, fireHeld_);
    leon::net::SetInputButton(lastLocalCmd_, leon::net::InputButtons::Reload, reloadRequested_);
    leon::net::SetInputButton(lastLocalCmd_, leon::net::InputButtons::Use, useRequested_);

    return move;
}

void ZombiesPlayerController::UpdateCamera(leon::Engine& engine, float deltaTime) {
    ZombiesCharacter* character = GetZombiesCharacter();
    if (character == nullptr || !IsLocalController()) {
        return;
    }
    character->SpringArm().ApplyToCamera(engine.GetCamera(), character->GetActorLocation(),
                                         deltaTime);
}

void ZombiesPlayerController::ApplyRemoteInput(const leon::net::InputCmdMsg& cmd) {
    if (cmd.jump != 0) {
        pendingRemoteJumpLatch_ = true;
    }
    if (leon::net::HasInputButton(cmd, leon::net::InputButtons::Reload)) {
        pendingRemoteReloadLatch_ = true;
    }
    if (leon::net::HasInputButton(cmd, leon::net::InputButtons::Use)) {
        pendingRemoteUseLatch_ = true;
    }
    lastRemoteFireHeld_ = leon::net::HasInputButton(cmd, leon::net::InputButtons::Fire);
    pendingRemote_ = cmd;
    hasPendingRemote_ = true;
}

leon::net::InputCmdMsg ZombiesPlayerController::ConsumeLocalInputCmd() {
    return lastLocalCmd_;
}

leon::net::InputCmdMsg ZombiesPlayerController::MakeIdleInputCmd() {
    ClearCombatInput();
    lastLocalCmd_.type = static_cast<std::uint8_t>(leon::net::ENetMsg::InputCmd);
    lastLocalCmd_.seq = ++localSeq_;
    return lastLocalCmd_;
}

} // namespace game
