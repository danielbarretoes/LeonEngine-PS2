#pragma once

#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>

namespace game {

class FurytoonCharacter;

/// Arena brawler input (shared top-down camera). LMB light / RMB heavy / Space jump.
class FurytoonPlayerController final : public leon::PlayerController {
public:
    [[nodiscard]] FurytoonCharacter* GetFurytoonCharacter() const;

    glm::vec3 TickInput(leon::Engine& engine) override;
    void UpdateCamera(leon::Engine& engine, float deltaTime) override;

    void ApplyRemoteInput(const leon::net::InputCmdMsg& cmd);
    [[nodiscard]] leon::net::InputCmdMsg ConsumeLocalInputCmd();
    [[nodiscard]] leon::net::InputCmdMsg MakeIdleInputCmd();

    [[nodiscard]] bool ConsumeLightAttack();
    [[nodiscard]] bool ConsumeHeavyAttack();
    void ClearCombatInput();

private:
    leon::net::InputCmdMsg pendingRemote_{};
    bool hasPendingRemote_ = false;
    bool pendingRemoteJumpLatch_ = false;
    bool pendingLightLatch_ = false;
    bool pendingHeavyLatch_ = false;

    bool lightRequested_ = false;
    bool heavyRequested_ = false;

    leon::net::InputCmdMsg lastLocalCmd_{};
    std::uint32_t localSeq_ = 0;
    int localJumpRepeatFrames_ = 0;
};

} // namespace game
