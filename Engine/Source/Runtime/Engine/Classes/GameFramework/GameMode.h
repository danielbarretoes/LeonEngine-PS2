#pragma once

#include <glm/vec3.hpp>

#include <algorithm>
#include "GameFramework/GameState.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Level/LevelCatalog.h"
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>


class Character;
class Engine;
class PlayerController;

/// Pluggable level gameplay rules (Unreal-style `AGameModeBase` / `AGameMode`).
/// Packs subclass this; the engine never includes pack headers.
class GameMode {
public:
    virtual ~GameMode() = default;

    GameMode(const GameMode&) = delete;
    GameMode& operator=(const GameMode&) = delete;
    GameMode(GameMode&&) = delete;
    GameMode& operator=(GameMode&&) = delete;

    /// Stable id matched against level JSON `"gameMode": "<id>"`.
    [[nodiscard]] virtual const char* Id() const = 0;

    /// Prefer `gameModeId == Id()`. Entry name/path may be used as a soft fallback.
    [[nodiscard]] virtual bool Matches(const LevelEntry& entry,
                                       const std::string& gameModeId) const = 0;

    virtual void OnEnter(Engine& engine, const std::string& levelPath) = 0;
    virtual void OnExit(Engine& engine) = 0;
    virtual void Tick(Engine& engine, float deltaTime) = 0;

    /// Unreal `InitGameState` — configure / replace GameState after construction.
    virtual void InitGameState() {}

    /// Unreal `StartMatch` — authority begins gameplay; notifies GameState.
    virtual void StartMatch() { GetGameState().HandleMatchHasStarted(); }
    /// Unreal `EndMatch`.
    virtual void EndMatch() { GetGameState().HandleMatchHasEnded(); }

    /// Unreal `AGameModeBase::PostLogin` — registers PlayerState on GameState::PlayerArray.
    virtual void PostLogin(PlayerController& newPlayer);
    /// Unreal `AGameModeBase::Logout` — removes PlayerState from GameState::PlayerArray.
    virtual void Logout(PlayerController& exiting);
    /// Unreal `RestartPlayer` — spawn/possess pawn at a PlayerStart (packs override).
    virtual void RestartPlayer(PlayerController& /*newPlayer*/) {}
    /// Unreal `HandleStartingNewPlayer` — default calls RestartPlayer.
    virtual void HandleStartingNewPlayer(PlayerController& newPlayer) { RestartPlayer(newPlayer); }

    /// Unreal `GetNumPlayers` (from GameState::PlayerArray).
    [[nodiscard]] int GetNumPlayers() const { return GetGameState().GetNumPlayers(); }

    [[nodiscard]] World& GetWorld() { return world_; }
    [[nodiscard]] const World& GetWorld() const { return world_; }

    /// Recreate World PhysScene backend (clears bodies). Prefer before RegisterBodiesFromLevel.
    void SetPhysicsBackend(EPhysicsBackend backend) { world_.SetPhysicsBackend(backend); }

    [[nodiscard]] GameState& GetGameState() { return *gameState_; }
    [[nodiscard]] const GameState& GetGameState() const { return *gameState_; }

    /// Unreal `GetGameState<T>()`.
    template <typename T>
    [[nodiscard]] T* GetGameState() {
        static_assert(std::is_base_of_v<GameState, T>, "T must derive from GameState");
        return dynamic_cast<T*>(gameState_.get());
    }
    template <typename T>
    [[nodiscard]] const T* GetGameState() const {
        static_assert(std::is_base_of_v<GameState, T>, "T must derive from GameState");
        return dynamic_cast<const T*>(gameState_.get());
    }

    /// Replace the GameState instance (e.g. pack-specific subclass). Calls InitGameState.
    template <typename T, typename... Args>
    T* SetGameState(Args&&... args) {
        static_assert(std::is_base_of_v<GameState, T>, "T must derive from GameState");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        gameState_ = std::move(owned);
        InitGameState();
        return raw;
    }

    /// Unreal `UWorld::ServerTravel` / GameMode travel — load map on authority (or local).
    [[nodiscard]] bool ServerTravel(Engine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});
    /// Unreal `APlayerController::ClientTravel` — load map on a joining/remote client.
    [[nodiscard]] bool ClientTravel(Engine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});

    /// Min PlayerStart Y, or 0 if none.
    [[nodiscard]] static float EstimateFloorY(const Level& level);
    /// Soft XZ walk clamp from static mesh extents (clamped 20–120).
    [[nodiscard]] static float EstimateWalkBounds(const Level& level);

protected:
    GameMode() : gameState_(std::make_unique<GameState>()) {}

    /// Framework helper: Level collision meshes → World PhysScene (not pack business logic).
    void RegisterBodiesFromLevel(const Level& level) { GetWorld().RegisterBodiesFromLevel(level); }

    /// Unreal `FindPlayerStart` — resolve spawn transform (`slot` picks among starts).
    [[nodiscard]] bool FindPlayerStart(const Level& level, glm::vec3& outLocation,
                                       float& outYawDegrees, int slot = 0) const {
        const auto& starts = level.PlayerStarts();
        if (starts.empty()) {
            outLocation = {0.0f, 0.0f, 0.0f};
            outYawDegrees = 0.0f;
            return false;
        }
        const int index = std::clamp(slot, 0, static_cast<int>(starts.size()) - 1);
        const PlayerStart& start = starts[static_cast<std::size_t>(index)];
        outLocation = start.transform.position;
        outYawDegrees = start.transform.rotationDegrees.y;
        return true;
    }

    // Flow: Match enter — bodies + nav bake
    void PrepareMatchWorld(Engine& engine, float& outFloorY, float& outWalkBounds,
                           EPhysicsBackend backend = EPhysicsBackend::Jolt);
    void RebuildNavigation(Engine& engine, float floorY, float walkBounds);
    void SnapCharacterToFloor(Character& character, glm::vec3& inOutFeet, float floorY) const;

private:
    World world_;
    std::unique_ptr<GameState> gameState_;
};

