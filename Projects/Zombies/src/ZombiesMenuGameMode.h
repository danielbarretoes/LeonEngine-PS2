#pragma once

#include <leon/Gameplay.h>
#include <string>

namespace game {

/// Main Menu (Unreal Game Default Map). Opens net session then Server/ClientTravel -> Lobby.
class ZombiesMenuGameMode final : public leon::GameMode {
public:
    [[nodiscard]] const char* Id() const override { return "zombies-menu"; }

    [[nodiscard]] bool Matches(const leon::LevelEntry& /*entry*/,
                               const std::string& gameModeId) const override {
        return gameModeId == Id() || gameModeId == "MainMenu";
    }

    void OnEnter(leon::Engine& engine, const std::string& levelPath) override;
    void OnExit(leon::Engine& engine) override;
    void Tick(leon::Engine& engine, float deltaTime) override;

private:
    void rebuildMenu();
    void activate(leon::Engine& engine, const std::string& itemId);
    void cancelPendingJoin(leon::Engine& engine, const std::string& reason);
    void syncJoinProgressBar();

    leon::Engine* engine_ = nullptr;
    std::string levelPath_;
    leon::ImageWidget* menuBackdrop_ = nullptr;
    leon::VerticalBoxWidget* menu_ = nullptr;
    leon::ProgressBarWidget* joinProgress_ = nullptr;
    bool backKeyWasDown_ = false;
    /// Join waits for NetDriver connect on MainMenu before ClientTravel -> Lobby.
    bool pendingJoin_ = false;
    float pendingJoinSeconds_ = 0.0f;
};

} // namespace game
