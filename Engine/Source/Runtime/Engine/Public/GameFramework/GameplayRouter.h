#pragma once

#include <cstddef>
#include "GameFramework/GameModeBase.h"
#include "Level/LevelDirector.h"
#include <memory>
#include <vector>


/// Picks the GameMode for the active level (Unreal-style GameMode Override).
/// Levels may set `"gameMode": "<id>"`. Empty → default mode.
class ENGINE_API FGameplayRouter {
public:
    void AddMode(std::unique_ptr<AGameModeBase> Mode);

    /// Fallback when the level has no override (or id is empty / "Default").
    void SetDefaultMode(std::unique_ptr<AGameModeBase> Mode);

    /// Call once per frame after `FLevelDirector::Update`.
    void Update(UGameEngine& Engine, const FLevelDirector& Director, float DeltaTime);

    [[nodiscard]] AGameModeBase* GetActive() const { return Active; }
    [[nodiscard]] AGameModeBase* GetDefaultMode() const { return DefaultMode.get(); }

private:
    void SyncActiveMode(UGameEngine& Engine, const FLevelDirector& Director);

    std::vector<std::unique_ptr<AGameModeBase>> Modes;
    std::unique_ptr<AGameModeBase> DefaultMode;
    AGameModeBase* Active = nullptr;
    std::size_t BoundCatalogIndex = static_cast<std::size_t>(-1);
};

