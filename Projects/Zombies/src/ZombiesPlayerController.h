#pragma once

#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>

namespace game {

class ZombiesCharacter;

/// Local FPS input or remote InputCmd on the listen/dedicated host.
/// Hold LMB to auto-fire; GameMode applies fire-rate cadence on authority. R = reload.
class ZombiesPlayerController final : public leon::PlayerController {
public:
    [[nodiscard]] ZombiesCharacter* GetZombiesCharacter() const;

    glm::vec3 TickInput(leon::Engine& engine) override;
    void UpdateCamera(leon::Engine& engine, float deltaTime) override;

    void ApplyRemoteInput(const leon::net::InputCmdMsg& cmd);
    [[nodiscard]] leon::net::InputCmdMsg ConsumeLocalInputCmd();
    /// Zero move/jump/fire, keep look -- used while the local pause menu is open.
    [[nodiscard]] leon::net::InputCmdMsg MakeIdleInputCmd();

    /// Trigger held this frame (local LMB or sticky remote InputCmd::fire).
    [[nodiscard]] bool IsFireHeld() const { return fireHeld_; }
    /// Reload edge this frame (local R or remote InputCmd::reload latch).
    [[nodiscard]] bool ConsumeReloadRequested();
    [[nodiscard]] bool ConsumeUseRequested();
    /// Clears fire/reload/jump sticky state (pause, logout, idle).
    void ClearCombatInput();

private:
    bool mouseLookSampleValid_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;

    leon::net::InputCmdMsg pendingRemote_{};
    bool hasPendingRemote_ = false;
    bool pendingRemoteJumpLatch_ = false;
    bool pendingRemoteReloadLatch_ = false;
    bool pendingRemoteUseLatch_ = false;
    bool fireHeld_ = false;
    /// Sticky remote trigger so auto-fire survives ticks without a new InputCmd.
    bool lastRemoteFireHeld_ = false;
    bool reloadRequested_ = false;
    bool reloadKeyWasDown_ = false;
    bool useRequested_ = false;
    bool useKeyWasDown_ = false;

    leon::net::InputCmdMsg lastLocalCmd_{};
    std::uint32_t localSeq_ = 0;
    int localJumpRepeatFrames_ = 0;
};

} // namespace game
