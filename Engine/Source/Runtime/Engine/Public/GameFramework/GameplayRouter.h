#pragma once

#include <cstddef>
#include "GameFramework/GameModeBase.h"
#include "Level/LevelDirector.h"
#include <memory>
#include <vector>


/// Picks the GameMode for the active level (Unreal-style GameMode Override).
/// Levels may set `"gameMode": "<id>"`. Empty → default mode.
class FGameplayRouter {
public:
    void AddMode(std::unique_ptr<AGameModeBase> mode);

    /// Fallback when the level has no override (or id is empty / "Default").
    void SetDefaultMode(std::unique_ptr<AGameModeBase> mode);

    /// Call once per frame after `FLevelDirector::Update`.
    void Update(UGameEngine& engine, const FLevelDirector& director, float deltaTime);

    [[nodiscard]] AGameModeBase* GetActive() const { return active_; }
    [[nodiscard]] AGameModeBase* GetDefaultMode() const { return defaultMode_.get(); }

private:
    void SyncActiveMode(UGameEngine& engine, const FLevelDirector& director);

    std::vector<std::unique_ptr<AGameModeBase>> modes_;
    std::unique_ptr<AGameModeBase> defaultMode_;
    AGameModeBase* active_ = nullptr;
    std::size_t boundCatalogIndex_ = static_cast<std::size_t>(-1);
};

