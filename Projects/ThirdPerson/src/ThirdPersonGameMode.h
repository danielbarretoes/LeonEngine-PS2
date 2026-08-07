#pragma once

#include <leon/Gameplay.h>

#include "ThirdPersonPlayerController.h"

namespace game {

/// GameMode: PlayerStart → ThirdPersonCharacter (Bot.lchar) + ThirdPersonPlayerController.
class ThirdPersonGameMode final : public leon::GameMode {
public:
    [[nodiscard]] const char* Id() const override { return "third-person"; }

    [[nodiscard]] bool Matches(const leon::LevelEntry& entry,
                               const std::string& gameModeId) const override;

    void OnEnter(leon::Engine& engine, const std::string& levelPath) override;
    void OnExit(leon::Engine& engine) override;
    void Tick(leon::Engine& engine, float deltaTime) override;

private:
    /// Cooked Bot pack under the project `assets/` (skel, mesh, anims, M_Bot, textures).
    static constexpr const char* kCharacterAsset = "assets/characters/bot/Bot.lchar";
    ThirdPersonPlayerController player_{};
};

} // namespace game
