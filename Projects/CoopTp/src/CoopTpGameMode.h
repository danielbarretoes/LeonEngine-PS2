#pragma once

#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>
#include <leon/net/RootReplication.h>
#include <string>
#include <string_view>
#include <vector>

#include "CoopTpGameState.h"
#include "CoopTpPlayerController.h"

namespace game {

class CoopTpCharacter;

/// Coop match GameMode (Courtyard / Rooftops). Front-end: coop-menu / coop-lobby.
/// Login: LoginPlayer → PostLogin (PlayerArray) → HandleStartingNewPlayer → RestartPlayer.
/// Session options live on CoopGameInstance (survive travel); match flags on GameState.
/// AI: stand on NavBlocker plate -> spawn 2 chase bots (snapshot slots >= kMaxPlayers).
class CoopTpGameMode final : public leon::GameMode {
public:
    CoopTpGameMode() { SetGameState<CoopTpGameState>(); }

    [[nodiscard]] const char* Id() const override { return "coop-tp"; }

    [[nodiscard]] bool Matches(const leon::LevelEntry& entry,
                               const std::string& gameModeId) const override;

    void InitGameState() override;
    void OnEnter(leon::Engine& engine, const std::string& levelPath) override;
    void OnExit(leon::Engine& engine) override;
    void Tick(leon::Engine& engine, float deltaTime) override;

    void PostLogin(leon::PlayerController& newPlayer) override;
    void Logout(leon::PlayerController& exiting) override;
    void RestartPlayer(leon::PlayerController& newPlayer) override;

private:
    [[nodiscard]] CoopTpGameState& CoopGameState();
    [[nodiscard]] const CoopTpGameState& CoopGameState() const;

    void setupNetCallbacks(leon::Engine& engine);
    void handlePacket(leon::Engine& engine, int peerSlot, const std::uint8_t* data,
                      std::size_t size);
    void tryHost(leon::Engine& engine);
    void tryDedicated(leon::Engine& engine);
    void tryJoin(leon::Engine& engine, const std::string& address);
    void beginStandaloneMatch(leon::Engine& engine);
    void beginDedicatedMatch(leon::Engine& engine);
    void spawnAllForListenServer(leon::Engine& engine);
    void spawnRemotePlayer(leon::Engine& engine, int playerSlot);
    void spawnAllForClient(leon::Engine& engine, std::uint8_t localSlot);
    /// Unreal-like accept path: PlayerState → PostLogin → HandleStartingNewPlayer.
    void LoginPlayer(int playerSlot, bool isLocalController);
    /// Client Welcome/Travel complete: session flags + spawn (single path).
    void FinishClientJoin(leon::Engine& engine, std::uint8_t localSlot,
                          std::string_view hostMapName);
    /// After map load on authority/client (soft re-enter; not true UE seamless).
    void OnTravelFinished(leon::Engine& engine);
    /// Late-join / map change notify only (not every soft re-enter).
    void NotifyClientsServerTravel(leon::Engine& engine);
    [[nodiscard]] bool ClientTravelToHostMap(leon::Engine& engine, std::string_view hostMapName);
    void tickHost(leon::Engine& engine, float deltaTime);
    void tickClient(leon::Engine& engine, float deltaTime);
    void tickDedicatedSpectator(leon::Engine& engine, float deltaTime);
    void sendSnapshot(leon::Engine& engine);
    void applySnapshot(leon::Engine& engine, const std::uint8_t* data, std::size_t size);
    void updateNetHud(leon::Engine& engine);
    void updateScoreboard(leon::Engine& engine, bool tabHeld);
    [[nodiscard]] std::string buildScoreboardText(const leon::Engine& engine) const;
    void openPauseMenu(leon::Engine& engine);
    void closePauseMenu(leon::Engine& engine);
    void returnToMenu(leon::Engine& engine);
    void refreshLevelIdentity(const leon::Engine& engine, const std::string& levelPath);
    void logoutAllPlayers();
    void syncSessionAddresses(leon::Engine& engine);
    /// Register level bodies + SyncFromLevel so spawn QuerySupportY sees floors.
    void prepareMatchPhysics(leon::Engine& engine);
    /// floorY / walkBounds from PlayerStarts + static mesh extents (Rooftops vs Courtyard).
    void configurePawnForLevel(CoopTpCharacter& character, const leon::Level& level) const;
    /// Snap feet onto QuerySupportY after physics sync (Unreal AdjustFloorHeight lite).
    void snapPawnToFloor(CoopTpCharacter& character, glm::vec3& inOutFeet) const;

    /// Orange pressure plate (tag NavBlocker); creates one if the level has none.
    void EnsureAISpawnPlate(leon::Engine& engine);
    void TickAISpawnPlate(leon::Engine& engine);
    [[nodiscard]] bool IsCharacterOnAISpawnPlate(const CoopTpCharacter& character,
                                                 const leon::Level& level) const;

    /// Authority: spawn chase bots. Clients create proxies from snapshot slots.
    void SpawnAIWave(leon::Engine& engine, int count);
    void ClearAIPawns(bool destroyPawns);
    void TickAIControllers(float deltaTime);
    [[nodiscard]] CoopTpCharacter* EnsureClientAIProxy(leon::Engine& engine, int aiIndex);
    [[nodiscard]] CoopTpCharacter* FindNearestPlayerCharacter(const glm::vec3& from) const;
    [[nodiscard]] static int AISnapshotSlot(int aiIndex) {
        return leon::net::kMaxPlayers + aiIndex;
    }
    [[nodiscard]] static int AIIndexFromSnapshotSlot(int slot) {
        return slot - leon::net::kMaxPlayers;
    }

    [[nodiscard]] std::string GetMapName() const;
    [[nodiscard]] CoopTpCharacter* characterAt(int slot) const;
    [[nodiscard]] int findPlayerStartIndex(const leon::Level& level, int slot) const;
    [[nodiscard]] int slotOf(const leon::PlayerController& pc) const;
    [[nodiscard]] int playerSlotFromPeer(int peerSlot) const;

    /// Cached from the active match level (updated in prepareMatchPhysics).
    float matchFloorY_ = 0.0f;
    float matchWalkBounds_ = 22.0f;

    leon::Engine* engine_ = nullptr;
    std::array<CoopTpPlayerController, leon::net::kMaxPlayers> players_{};
    std::array<CoopTpCharacter*, leon::net::kMaxPlayers> characters_{};
    std::array<leon::AIController, leon::net::kMaxAiPawns> aiControllers_{};
    std::array<CoopTpCharacter*, leon::net::kMaxAiPawns> aiPawns_{};
    leon::AIChaseBehavior chaseBehavior_{};
    /// True until a player stands on the AI spawn plate (authority).
    bool bAISpawnPlateArmed_ = true;
    int localSlot_ = 0;
    bool hostKeyWasDown_ = false;
    bool dedicatedKeyWasDown_ = false;
    bool clientKeyWasDown_ = false;
    bool localhostKeyWasDown_ = false;
    bool escapeWasDown_ = false;
    bool pauseMenuOpen_ = false;
    leon::VerticalBoxWidget* pauseMenu_ = nullptr;
    leon::TextBlockWidget* scoreboard_ = nullptr;
    /// Suppress re-entrant OnEnter while ClientTravel / ServerTravel loads a map.
    bool applyingTravel_ = false;
    std::array<std::uint32_t, leon::net::kMaxPlayers> remoteInputSeq_{};
    std::string levelPath_;
    std::string levelKey_;

    struct PawnTarget {
        glm::vec3 pos{0.0f};
        float yaw = 0.0f;
        float velY = 0.0f;
        float animBlend = 0.0f;
        float boomYaw = 0.0f;
        float boomPitch = 0.0f;
        bool grounded = true;
        bool valid = false;
    };
    std::array<PawnTarget, leon::net::kMaxPlayers> pawnTargets_{};
    std::array<PawnTarget, leon::net::kMaxAiPawns> aiPawnTargets_{};

    struct BodyTarget {
        std::size_t levelMeshIndex = 0;
        glm::vec3 pos{0.0f};
        glm::vec3 vel{0.0f};
        bool valid = false;
    };
    std::vector<BodyTarget> bodyTargets_;
};

} // namespace game
