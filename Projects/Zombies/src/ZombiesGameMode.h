#pragma once

#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <leon/Gameplay.h>
#include <leon/net/NetProtocol.h>
#include <string>
#include <string_view>
#include <vector>

#include "ZombieCharacter.h"
#include "ZombiesCrosshairWidget.h"
#include "ZombiesGameState.h"
#include "ZombiesInteract.h"
#include "ZombiesMatchHudWidget.h"
#include "ZombiesPlayerController.h"
#include "ZombiesPlayerState.h"

namespace game {

class ZombiesCharacter;

/// Zombies match GameMode (Town). Front-end: zombies-menu / zombies-lobby.
/// Loop: points → doors → wall guns → PaP (lava) → survive → ammo / perks → repeat.
class ZombiesGameMode final : public leon::GameMode {
public:
    ZombiesGameMode() { SetGameState<ZombiesGameState>(); }

    [[nodiscard]] const char* Id() const override { return "zombies-match"; }

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
    [[nodiscard]] ZombiesGameState& MatchGameState();
    [[nodiscard]] const ZombiesGameState& MatchGameState() const;

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
    void prepareMatchPhysics(leon::Engine& engine);
    void configurePawnForLevel(ZombiesCharacter& character, const leon::Level& level) const;
    void snapPawnToFloor(ZombiesCharacter& character, glm::vec3& inOutFeet) const;

    void CollectZombieSpawnPoints(const leon::Level& level);
    void StartNextRound(leon::Engine& engine);
    void SpawnZombieWave(leon::Engine& engine);
    void ClearAIPawns(bool destroyPawns);
    void TickRoundDirector(leon::Engine& engine, float deltaTime);
    void TickAIControllers(float deltaTime);
    [[nodiscard]] int CountAliveZombies() const;
    [[nodiscard]] ZombiesCharacter* EnsureClientAIProxy(leon::Engine& engine, int aiIndex);
    [[nodiscard]] ZombiesCharacter* FindNearestLivingPlayer(const glm::vec3& from) const;

    bool ApplyPointDamage(ZombiesCharacter& target, float damage, int instigatorSlot);
    void HandlePlayerDeath(leon::Engine& engine, int playerSlot);
    void CheckGameOver(leon::Engine& engine);
    /// Returns true if a round was consumed and a shot was fired.
    [[nodiscard]] bool ProcessFireForSlot(leon::Engine& engine, int playerSlot);
    void ProcessPlayerFires(leon::Engine& engine, float deltaTime);
    void ProcessPlayerReloads(leon::Engine& engine);
    void TickPlayerWeapons(float deltaTime);
    void PredictLocalShotFx(leon::Engine& engine, ZombiesCharacter& localCh, float deltaTime);
    void TickContactMelee(leon::Engine& engine, float deltaTime);
    void AppendPawnPrimitiveVisuals(leon::Engine& engine) const;
    void ensureMatchChrome(leon::Engine& engine);
    void destroyMatchChrome(leon::Engine& engine);
    void syncMatchHud(leon::Engine& engine);
    void SpawnShotFx(const glm::vec3& start, const glm::vec3& end, bool hitSomething);
    void TickShotFx(leon::Engine& engine, float deltaTime);
    [[nodiscard]] leon::DebugDraw* WeaponTraceDebugDraw(leon::Engine& engine) const;

    void CollectTownInteractables(const leon::Level& level);
    void RebuildNavAfterDoor(leon::Engine& engine);
    void TickLavaDamage(leon::Engine& engine, float deltaTime);
    void TickTownInteract(leon::Engine& engine);
    [[nodiscard]] int FindNearestInteractable(const glm::vec3& feet, float maxDist) const;
    [[nodiscard]] std::string FormatInteractPrompt(const ZombiesInteractable& buy) const;
    bool TryPurchaseInteractable(leon::Engine& engine, int playerSlot, int buyIndex);
    void ApplyPerkToCharacter(ZombiesPlayerState& ps, ZombiesCharacter& ch, EZombiesPerk perk);

    [[nodiscard]] static int AISnapshotSlot(int aiIndex) {
        return leon::net::kMaxPlayers + aiIndex;
    }
    [[nodiscard]] static int AIIndexFromSnapshotSlot(int slot) {
        return slot - leon::net::kMaxPlayers;
    }

    [[nodiscard]] std::string GetMapName() const;
    [[nodiscard]] ZombiesCharacter* characterAt(int slot) const;
    [[nodiscard]] int findPlayerStartIndex(const leon::Level& level, int slot) const;
    [[nodiscard]] int slotOf(const leon::PlayerController& pc) const;
    [[nodiscard]] int playerSlotFromPeer(int peerSlot) const;

    float matchFloorY_ = 0.0f;
    float matchWalkBounds_ = 40.0f;

    leon::Engine* engine_ = nullptr;
    std::array<ZombiesPlayerController, leon::net::kMaxPlayers> players_{};
    std::array<ZombiesCharacter*, leon::net::kMaxPlayers> characters_{};
    std::array<leon::AIController, leon::net::kMaxAiPawns> aiControllers_{};
    std::array<ZombiesCharacter*, leon::net::kMaxAiPawns> aiPawns_{};
    leon::AIChaseBehavior chaseBehavior_{};
    std::vector<glm::vec3> zombieSpawnPoints_;
    std::array<float, leon::net::kMaxAiPawns> zombieMeleeCooldown_{};
    std::array<float, leon::net::kMaxPlayers> playerFireCooldown_{};
    std::vector<ZombiesInteractable> townBuys_;
    std::vector<leon::PainCausingVolume> townPain_;
    std::string localInteractPrompt_;
    float lavaTickAccum_ = 0.0f;

    struct ShotFx {
        glm::vec3 start{0.0f};
        glm::vec3 end{0.0f};
        float life = 0.0f;
        bool hit = false;
    };
    std::vector<ShotFx> shotFx_;

    float intermissionRemaining_ = 0.0f;
    bool bMatchGameOver_ = false;

    int localSlot_ = 0;
    bool hostKeyWasDown_ = false;
    bool dedicatedKeyWasDown_ = false;
    bool clientKeyWasDown_ = false;
    bool localhostKeyWasDown_ = false;
    bool escapeWasDown_ = false;
    bool pauseMenuOpen_ = false;
    leon::VerticalBoxWidget* pauseMenu_ = nullptr;
    leon::TextBlockWidget* scoreboard_ = nullptr;
    ZombiesCrosshairWidget* crosshair_ = nullptr;
    ZombiesMatchHudWidget* matchHud_ = nullptr;
    bool applyingTravel_ = false;
    std::array<std::uint32_t, leon::net::kMaxPlayers> remoteInputSeq_{};
    /// Last fire bit sent on the client InputCmd (reliable only on rising edge).
    bool lastSentFire_ = false;
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
        float health = 100.0f;
        bool alive = true;
        int ammoInMag = 0;
        int ammoReserve = 0;
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
