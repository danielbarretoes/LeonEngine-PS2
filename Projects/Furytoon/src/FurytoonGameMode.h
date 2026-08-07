#pragma once

#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>
#include <string>
#include <string_view>
#include <vector>

#include "FurytoonGameState.h"
#include "FurytoonPlayerController.h"

namespace game {

class FurytoonCharacter;

/// Kitchen arena party fighter: up to 4 slots (humans + AI bots), light/heavy combos, stocks.
class FurytoonGameMode final : public leon::GameMode {
public:
    FurytoonGameMode() { SetGameState<FurytoonGameState>(); }

    [[nodiscard]] const char* Id() const override { return "furytoon-match"; }

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
    [[nodiscard]] FurytoonGameState& MatchGameState();
    [[nodiscard]] const FurytoonGameState& MatchGameState() const;

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
    void LoginPlayer(int playerSlot, bool isLocalController);
    void FinishClientJoin(leon::Engine& engine, std::uint8_t localSlot,
                          std::string_view hostMapName);
    void OnTravelFinished(leon::Engine& engine);
    void NotifyClientsServerTravel(leon::Engine& engine);
    [[nodiscard]] bool ClientTravelToHostMap(leon::Engine& engine, std::string_view hostMapName);
    void tickHost(leon::Engine& engine, float deltaTime);
    void tickClient(leon::Engine& engine, float deltaTime);
    void sendSnapshot(leon::Engine& engine);
    void applySnapshot(leon::Engine& engine, const std::uint8_t* data, std::size_t size);
    void updateHud(leon::Engine& engine);
    void openPauseMenu(leon::Engine& engine);
    void closePauseMenu(leon::Engine& engine);
    void returnToMenu(leon::Engine& engine);
    void refreshLevelIdentity(const leon::Engine& engine, const std::string& levelPath);
    void logoutAllPlayers();
    void destroyAllFighterMeshes(leon::Engine& engine);
    void syncSessionAddresses(leon::Engine& engine);
    void prepareMatchPhysics(leon::Engine& engine);
    void configurePawnForLevel(FurytoonCharacter& character, const leon::Level& level) const;
    void snapPawnToFloor(FurytoonCharacter& character, glm::vec3& inOutFeet) const;

    void FillEmptySlotsWithBots(leon::Engine& engine);
    void TickBotAI(float deltaTime);
    void ProcessCombat(leon::Engine& engine, float deltaTime);
    void ProcessAttacksForSlot(leon::Engine& engine, int slot);
    void ApplyMeleeHit(leon::Engine& engine, int attackerSlot);
    void HandleFighterKO(leon::Engine& engine, int victimSlot, int attackerSlot);
    void AppendFighterVisuals(leon::Engine& engine) const;
    void UpdateSharedArenaCamera(leon::Engine& engine, float deltaTime);
    void EnsureFighterAccent(int slot, FurytoonCharacter& character);
    void CheckMatchOver(leon::Engine& engine);

    [[nodiscard]] static int BotSnapshotSlot(int botIndex) {
        return leon::net::kMaxPlayers + botIndex;
    }
    [[nodiscard]] std::string GetMapName() const;
    [[nodiscard]] FurytoonCharacter* characterAt(int slot) const;
    [[nodiscard]] int findPlayerStartIndex(const leon::Level& level, int slot) const;
    [[nodiscard]] int slotOf(const leon::PlayerController& pc) const;
    [[nodiscard]] int playerSlotFromPeer(int peerSlot) const;
    [[nodiscard]] int CountLivingFighters() const;

    float matchFloorY_ = 0.0f;
    float matchWalkBounds_ = 28.0f;

    leon::Engine* engine_ = nullptr;
    std::array<FurytoonPlayerController, leon::net::kMaxPlayers> players_{};
    std::array<FurytoonCharacter*, leon::net::kMaxPlayers> characters_{};
    std::array<leon::AIController, leon::net::kMaxPlayers> botControllers_{};
    std::array<bool, leon::net::kMaxPlayers> slotIsBot_{};
    leon::AIChaseBehavior chaseBehavior_{};

    int localSlot_ = 0;
    bool hostKeyWasDown_ = false;
    bool dedicatedKeyWasDown_ = false;
    bool clientKeyWasDown_ = false;
    bool localhostKeyWasDown_ = false;
    bool escapeWasDown_ = false;
    bool pauseMenuOpen_ = false;
    bool bMatchOver_ = false;
    leon::VerticalBoxWidget* pauseMenu_ = nullptr;
    bool applyingTravel_ = false;
    std::array<std::uint32_t, leon::net::kMaxPlayers> remoteInputSeq_{};
    std::string levelPath_;
    std::string levelKey_;

    leon::ArenaCameraState arenaCamera_{{0.0f, 0.5f, 0.0f}, 18.0f};

    struct PawnTarget {
        glm::vec3 pos{0.0f};
        float yaw = 0.0f;
        float velY = 0.0f;
        float animBlend = 0.0f;
        float boomYaw = 0.0f;
        float boomPitch = 0.0f;
        bool grounded = true;
        float health = 100.0f;
        bool alive = true;
        bool valid = false;
    };
    std::array<PawnTarget, leon::net::kMaxPlayers> pawnTargets_{};
};

} // namespace game
