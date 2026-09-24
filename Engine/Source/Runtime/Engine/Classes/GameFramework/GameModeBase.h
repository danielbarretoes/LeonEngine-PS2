#pragma once

#include <glm/vec3.hpp>

#include <algorithm>
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Level/LevelCatalog.h"
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>


class ACharacter;
class UGameEngine;
class APlayerController;

/// Pluggable level gameplay rules (Unreal-style `AGameModeBase` / `AGameMode`).
/// Packs subclass this; the engine never includes pack headers.
class AGameModeBase {
public:
    virtual ~AGameModeBase() = default;

    AGameModeBase(const AGameModeBase&) = delete;
    AGameModeBase& operator=(const AGameModeBase&) = delete;
    AGameModeBase(AGameModeBase&&) = delete;
    AGameModeBase& operator=(AGameModeBase&&) = delete;

    /// Stable id matched against level JSON `"gameMode": "<id>"`.
    [[nodiscard]] virtual const char* Id() const = 0;

    /// Prefer `gameModeId == Id()`. Entry name/path may be used as a soft fallback.
    [[nodiscard]] virtual bool Matches(const FLevelEntry& entry,
                                       const std::string& gameModeId) const = 0;

    virtual void OnEnter(UGameEngine& engine, const std::string& levelPath) = 0;
    virtual void OnExit(UGameEngine& engine) = 0;
    virtual void Tick(UGameEngine& engine, float deltaTime) = 0;

    /// Unreal `InitGameState` — configure / replace GameState after construction.
    virtual void InitGameState() {}

    /// Unreal `StartMatch` — authority begins gameplay; notifies GameState.
    virtual void StartMatch() { GetGameState().HandleMatchHasStarted(); }
    /// Unreal `EndMatch`.
    virtual void EndMatch() { GetGameState().HandleMatchHasEnded(); }

    /// Unreal `AGameModeBase::PostLogin` — registers PlayerState on GameState::PlayerArray.
    virtual void PostLogin(APlayerController& newPlayer);
    /// Unreal `AGameModeBase::Logout` — removes PlayerState from GameState::PlayerArray.
    virtual void Logout(APlayerController& exiting);
    /// Unreal `RestartPlayer` — spawn/possess pawn at a FPlayerStart (packs override).
    virtual void RestartPlayer(APlayerController& /*newPlayer*/) {}
    /// Unreal `HandleStartingNewPlayer` — default calls RestartPlayer.
    virtual void HandleStartingNewPlayer(APlayerController& newPlayer) { RestartPlayer(newPlayer); }

    /// Unreal `GetNumPlayers` (from GameState::PlayerArray).
    [[nodiscard]] int GetNumPlayers() const { return GetGameState().GetNumPlayers(); }

    [[nodiscard]] UWorld& GetWorld() { return world_; }
    [[nodiscard]] const UWorld& GetWorld() const { return world_; }

    /// Recreate World FPhysScene backend (clears bodies). Prefer before RegisterBodiesFromLevel.
    void SetPhysicsBackend(EPhysicsBackend backend) { world_.SetPhysicsBackend(backend); }

    [[nodiscard]] AGameStateBase& GetGameState() { return *gameState_; }
    [[nodiscard]] const AGameStateBase& GetGameState() const { return *gameState_; }

    /// Unreal `GetGameState<T>()`.
    template <typename T>
    [[nodiscard]] T* GetGameState() {
        static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
        return dynamic_cast<T*>(gameState_.get());
    }
    template <typename T>
    [[nodiscard]] const T* GetGameState() const {
        static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
        return dynamic_cast<const T*>(gameState_.get());
    }

    /// Replace the GameState instance (e.g. pack-specific subclass). Calls InitGameState.
    template <typename T, typename... ArgsType>
    T* SetGameState(ArgsType&&... args) {
        static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
        auto owned = std::make_unique<T>(std::forward<ArgsType>(args)...);
        T* raw = owned.get();
        gameState_ = std::move(owned);
        InitGameState();
        return raw;
    }

    /// Unreal `UWorld::ServerTravel` / GameMode travel — load map on authority (or local).
    [[nodiscard]] bool ServerTravel(UGameEngine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});
    /// Unreal `APlayerController::ClientTravel` — load map on a joining/remote client.
    [[nodiscard]] bool ClientTravel(UGameEngine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});

    /// Min FPlayerStart Y, or 0 if none.
    [[nodiscard]] static float EstimateFloorY(const ULevel& level);
    /// Soft XZ walk clamp from static mesh extents (clamped 20–120).
    [[nodiscard]] static float EstimateWalkBounds(const ULevel& level);

protected:
    AGameModeBase() : gameState_(std::make_unique<AGameStateBase>()) {}

    /// Framework helper: Level collision meshes → World FPhysScene (not pack business logic).
    void RegisterBodiesFromLevel(const ULevel& level) { GetWorld().RegisterBodiesFromLevel(level); }

    /// Unreal `FindPlayerStart` — resolve spawn transform (`slot` picks among starts).
    [[nodiscard]] bool FindPlayerStart(const ULevel& level, glm::vec3& outLocation,
                                       float& outYawDegrees, int slot = 0) const {
        const auto& starts = level.PlayerStarts();
        if (starts.empty()) {
            outLocation = {0.0f, 0.0f, 0.0f};
            outYawDegrees = 0.0f;
            return false;
        }
        const int index = std::clamp(slot, 0, static_cast<int>(starts.size()) - 1);
        const FPlayerStart& start = starts[static_cast<std::size_t>(index)];
        outLocation = start.transform.Position;
        outYawDegrees = start.transform.RotationDegrees.y;
        return true;
    }

    // Flow: Match enter — bodies + nav bake
    void PrepareMatchWorld(UGameEngine& engine, float& outFloorY, float& outWalkBounds,
                           EPhysicsBackend backend = EPhysicsBackend::Jolt);
    void RebuildNavigation(UGameEngine& engine, float floorY, float walkBounds);
    void SnapCharacterToFloor(ACharacter& character, glm::vec3& inOutFeet, float floorY) const;

private:
    UWorld world_;
    std::unique_ptr<AGameStateBase> gameState_;
};

