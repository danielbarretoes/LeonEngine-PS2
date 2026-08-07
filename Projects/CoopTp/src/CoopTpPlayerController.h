#pragma once

#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>

namespace game {

class CoopTpCharacter;

/// Local input or remote InputCmd on the listen/dedicated host.
class CoopTpPlayerController final : public leon::PlayerController {
public:
    [[nodiscard]] CoopTpCharacter* GetCoopCharacter() const;

    glm::vec3 TickInput(leon::Engine& engine) override;
    void UpdateCamera(leon::Engine& engine, float deltaTime) override;

    void ApplyRemoteInput(const leon::net::InputCmdMsg& cmd);
    [[nodiscard]] leon::net::InputCmdMsg ConsumeLocalInputCmd();
    /// Zero move/jump, keep look — used while the local pause menu is open.
    [[nodiscard]] leon::net::InputCmdMsg MakeIdleInputCmd();

private:
    bool mouseLookSampleValid_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;

    leon::net::InputCmdMsg pendingRemote_{};
    bool hasPendingRemote_ = false;
    /// Jump edge survives later move-only InputCmds in the same Poll (or packet loss).
    bool pendingRemoteJumpLatch_ = false;

    leon::net::InputCmdMsg lastLocalCmd_{};
    std::uint32_t localSeq_ = 0;
    /// Re-assert jump for a few cmds so unreliable drop / overwrite is less likely.
    int localJumpRepeatFrames_ = 0;
};

} // namespace game
