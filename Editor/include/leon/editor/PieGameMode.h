#pragma once

#include <leon/gameplay/GameMode.h>
#include <leon/gameplay/PlayerController.h>
#include <memory>

namespace leon::editor {

/// Edit-time PIE third-person preview only (not shipping).
/// Real GameMode / Character / PC live in Templates/ThirdPerson → project `src/`.
class PieGameMode final : public leon::GameMode {
public:
    [[nodiscard]] const char* Id() const override { return "Pie"; }

    [[nodiscard]] bool Matches(const leon::LevelEntry& /*entry*/,
                               const std::string& gameModeId) const override {
        return gameModeId == Id() || gameModeId == "third-person" || gameModeId.empty();
    }

    void OnEnter(leon::Engine& engine, const std::string& levelPath) override;
    void OnExit(leon::Engine& engine) override;
    void Tick(leon::Engine& engine, float deltaTime) override;

private:
    std::unique_ptr<leon::PlayerController> player_;
    float painTickAccum_ = 0.0f;
};

} // namespace leon::editor
