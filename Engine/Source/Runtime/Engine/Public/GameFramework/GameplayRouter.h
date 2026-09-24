#pragma once

#include <cstddef>
#include "GameFramework/GameMode.h"
#include "Level/LevelDirector.h"
#include <memory>
#include <vector>

namespace leon {

/// Picks the GameMode for the active level (Unreal-style GameMode Override).
/// Levels may set `"gameMode": "<id>"`. Empty → default mode.
class GameplayRouter {
public:
    void AddMode(std::unique_ptr<GameMode> mode);

    /// Fallback when the level has no override (or id is empty / "Default").
    void SetDefaultMode(std::unique_ptr<GameMode> mode);

    /// Call once per frame after `LevelDirector::Update`.
    void Update(Engine& engine, const LevelDirector& director, float deltaTime);

    [[nodiscard]] GameMode* GetActive() const { return active_; }
    [[nodiscard]] GameMode* GetDefaultMode() const { return defaultMode_.get(); }

private:
    void SyncActiveMode(Engine& engine, const LevelDirector& director);

    std::vector<std::unique_ptr<GameMode>> modes_;
    std::unique_ptr<GameMode> defaultMode_;
    GameMode* active_ = nullptr;
    std::size_t boundCatalogIndex_ = static_cast<std::size_t>(-1);
};

} // namespace leon
