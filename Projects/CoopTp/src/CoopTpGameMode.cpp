#include "CoopTpGameMode.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <leon/core/Ascii.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>
#include <leon/gameplay/NavigationSystem.h>
#include <leon/level/Level.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetUtil.h>
#include <leon/net/RootReplication.h>
#include <leon/net/SnapshotCodec.h>
#include <leon/physics/BodyInstance.h>
#include <leon/physics/CollisionShape.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/StaticMesh.h>
#include <string>
#include <string_view>
#include <vector>

#include "CoopGameInstance.h"
#include "CoopTpCharacter.h"
#include "CoopTpPlayerState.h"

namespace game {
namespace {

[[nodiscard]] CoopGameInstance& Session(leon::Engine& engine) {
    return CoopGameInstance::Get(engine.GetGameInstance());
}

[[nodiscard]] const CoopGameInstance& Session(const leon::Engine& engine) {
    return CoopGameInstance::Get(engine.GetGameInstance());
}

constexpr const char* kBotCharacter = "assets/characters/bot/Bot.lchar";

float ExpAlpha(float speed, float dt) {
    if (speed <= 0.0f || dt <= 0.0f) {
        return 1.0f;
    }
    return 1.0f - std::exp(-speed * dt);
}

float LerpAngleDegrees(float from, float to, float t) {
    float delta = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    if (delta <= -180.0f) {
        delta += 360.0f;
    }
    return from + delta * t;
}

[[nodiscard]] bool LevelKeysMatch(std::string_view a, std::string_view b) {
    return leon::AsciiToLower(a) == leon::AsciiToLower(b);
}

constexpr const char* kAISpawnPlateTag = leon::NavTags::Blocker;

} // namespace

void CoopTpGameMode::InitGameState() {
    // Default GameState already constructed via SetGameState<CoopTpGameState> in ctor.
}

CoopTpGameState& CoopTpGameMode::CoopGameState() {
    CoopTpGameState* gs = GetGameState<CoopTpGameState>();
    return gs != nullptr ? *gs : static_cast<CoopTpGameState&>(GetGameState());
}

const CoopTpGameState& CoopTpGameMode::CoopGameState() const {
    const CoopTpGameState* gs = GetGameState<CoopTpGameState>();
    return gs != nullptr ? *gs : static_cast<const CoopTpGameState&>(GetGameState());
}

bool CoopTpGameMode::Matches(const leon::LevelEntry& /*entry*/,
                             const std::string& gameModeId) const {
    // Front-end maps use coop-menu / coop-lobby; only gameplay arenas claim coop-tp.
    return gameModeId == Id() || gameModeId == "CoopTp";
}

int CoopTpGameMode::findPlayerStartIndex(const leon::Level& level, int slot) const {
    const auto& starts = level.PlayerStarts();
    if (starts.empty()) {
        return -1;
    }
    return std::clamp(slot, 0, static_cast<int>(starts.size()) - 1);
}

CoopTpCharacter* CoopTpGameMode::characterAt(int slot) const {
    if (slot < 0 || slot >= leon::net::kMaxPlayers) {
        return nullptr;
    }
    return characters_[static_cast<std::size_t>(slot)];
}

int CoopTpGameMode::slotOf(const leon::PlayerController& pc) const {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (&players_[static_cast<std::size_t>(i)] == &pc) {
            return i;
        }
    }
    return pc.GetPlayerState().GetPlayerId();
}

int CoopTpGameMode::playerSlotFromPeer(int peerSlot) const {
    if (engine_ != nullptr && engine_->GetGameInstance().IsDedicatedServer()) {
        return peerSlot;
    }
    // Listen-server: single remote peer maps to player slot 1.
    return peerSlot + 1;
}

void CoopTpGameMode::refreshLevelIdentity(const leon::Engine& engine,
                                          const std::string& levelPath) {
    levelPath_ = levelPath;
    if (!engine.GetLevel().Name().empty()) {
        levelKey_ = engine.GetLevel().Name();
    } else if (!levelPath.empty()) {
        levelKey_ = std::filesystem::path(levelPath).stem().string();
    } else {
        levelKey_.clear();
    }
    CoopGameState().SetMapName(levelKey_);
}

std::string CoopTpGameMode::GetMapName() const {
    return levelKey_;
}

void CoopTpGameMode::NotifyClientsServerTravel(leon::Engine& engine) {
    CoopGameState().SetExpectedMapName(GetMapName());
    leon::net::SendTravelToPeers(engine.GetGameInstance().GetNetDriver(), GetMapName(),
                                 engine.GetGameInstance().IsDedicatedServer());
}

bool CoopTpGameMode::ClientTravelToHostMap(leon::Engine& engine, std::string_view hostMapName) {
    if (hostMapName.empty()) {
        return true;
    }
    if (LevelKeysMatch(GetMapName(), hostMapName)) {
        return true;
    }

    applyingTravel_ = true;
    const bool ok = ClientTravel(engine, hostMapName, levelPath_);
    applyingTravel_ = false;
    if (!ok) {
        engine.AddOnScreenDebugMessage("ClientTravel failed -- host map '" +
                                           std::string(hostMapName) + "' missing locally",
                                       6.0f, {1.0f, 0.35f, 0.3f});
        std::cerr << "CoopTp: ClientTravel to '" << hostMapName << "' failed\n";
        return false;
    }

    // Keep a filesystem hint for future sibling resolves (PIE / no LevelDirector).
    if (!levelPath_.empty()) {
        namespace fs = std::filesystem;
        const fs::path dir = fs::path(levelPath_).parent_path();
        const std::string needle = leon::AsciiToLower(hostMapName);
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec || !entry.is_regular_file()) {
                continue;
            }
            if (leon::AsciiToLower(entry.path().extension().string()) != ".llev") {
                continue;
            }
            if (leon::AsciiToLower(entry.path().stem().string()) == needle) {
                levelPath_ = entry.path().lexically_normal().string();
                break;
            }
        }
    }

    refreshLevelIdentity(engine, levelPath_);
    if (!LevelKeysMatch(GetMapName(), hostMapName) && !engine.GetLevel().Name().empty()) {
        levelKey_ = engine.GetLevel().Name();
    } else if (GetMapName().empty()) {
        levelKey_ = std::string(hostMapName);
    }
    CoopGameState().SetMapName(levelKey_);
    CoopGameState().SetExpectedMapName(std::string(hostMapName));

    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    RegisterBodiesFromLevel(engine.GetLevel());
    engine.AddOnScreenDebugMessage("ClientTravel -> '" + levelKey_ + "'", 4.0f,
                                   {0.45f, 0.95f, 1.0f});
    std::cout << "CoopTp: ClientTravel to '" << levelKey_ << "'\n";
    return true;
}

void CoopTpGameMode::OnTravelFinished(leon::Engine& engine) {
    bodyTargets_.clear();
    for (PawnTarget& target : pawnTargets_) {
        target = {};
    }
    CoopGameState().SetMapName(GetMapName());

    // Soft re-enter after travel: respawn only. Late-join Travel notify is OnPeerConnected.
    if (engine.GetGameInstance().IsDedicatedServer()) {
        beginDedicatedMatch(engine);
        const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
        for (int peer = 0; peer < peers && peer < leon::net::kMaxPlayers; ++peer) {
            spawnRemotePlayer(engine, peer);
        }
        return;
    }
    if (engine.GetGameInstance().IsListenServer()) {
        const bool hadPeer = engine.GetGameInstance().GetNetDriver().HasPeer();
        spawnAllForListenServer(engine);
        if (hadPeer) {
            spawnRemotePlayer(engine, 1);
        }
        return;
    }
    if (engine.GetGameInstance().IsClient() && Session(engine).IsClientWelcomed()) {
        spawnAllForClient(engine, static_cast<std::uint8_t>(localSlot_));
        return;
    }
    beginStandaloneMatch(engine);
}

void CoopTpGameMode::LoginPlayer(int playerSlot, bool isLocalController) {
    if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
        return;
    }
    CoopTpPlayerController& pc = players_[static_cast<std::size_t>(playerSlot)];
    // Re-login only: Logout clears PlayerArray + pawn before SetPlayerState (dangling PS).
    if (GetGameState().HasPlayerState(&pc.GetPlayerState()) || characterAt(playerSlot) != nullptr) {
        Logout(pc);
    }
    pc.SetPlayerState<CoopTpPlayerState>();
    pc.SetIsLocalController(isLocalController);
    pc.GetPlayerState().SetPlayerId(playerSlot);
    // Flow: PostLogin -> PlayerArray -> HandleStartingNewPlayer -> RestartPlayer.
    PostLogin(pc);
    HandleStartingNewPlayer(pc);
}

void CoopTpGameMode::logoutAllPlayers() {
    for (CoopTpPlayerController& pc : players_) {
        Logout(pc);
    }
    characters_.fill(nullptr);
}

void CoopTpGameMode::prepareMatchPhysics(leon::Engine& engine) {
    // Spawn plate before body register / nav bake. Slope ramps belong in authored .llev only.
    matchFloorY_ = EstimateFloorY(engine.GetLevel());
    EnsureAISpawnPlate(engine);
    PrepareMatchWorld(engine, matchFloorY_, matchWalkBounds_);
    bAISpawnPlateArmed_ = true;
    ClearAIPawns(false);
}

void CoopTpGameMode::configurePawnForLevel(CoopTpCharacter& character,
                                           const leon::Level& /*level*/) const {
    leon::CharacterMovement& move = character.GetCharacterMovement();
    // Infinite floor plane at match height (Courtyard 0 / Rooftops 4) -- not world Y=0.
    move.FloorY = matchFloorY_;
    move.WalkBounds = matchWalkBounds_;
}

void CoopTpGameMode::snapPawnToFloor(CoopTpCharacter& character, glm::vec3& inOutFeet) const {
    SnapCharacterToFloor(character, inOutFeet, matchFloorY_);
}

namespace {

void FillPawnSnap(leon::net::PawnSnap& snap, std::uint8_t slot, const CoopTpCharacter& ch) {
    snap = leon::net::CaptureCharacterRoot(slot, ch, ch.SpringArm().BoomYawDegrees,
                                           ch.SpringArm().BoomPitchDegrees);
}

} // namespace

void CoopTpGameMode::ClearAIPawns(bool destroyPawns) {
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        aiControllers_[static_cast<std::size_t>(i)].StopMovement();
        aiControllers_[static_cast<std::size_t>(i)].UnPossess();
        if (destroyPawns && aiPawns_[static_cast<std::size_t>(i)] != nullptr) {
            GetWorld().DestroyActor(aiPawns_[static_cast<std::size_t>(i)]);
        }
        aiPawns_[static_cast<std::size_t>(i)] = nullptr;
        aiPawnTargets_[static_cast<std::size_t>(i)] = {};
    }
}

CoopTpCharacter* CoopTpGameMode::FindNearestPlayerCharacter(const glm::vec3& from) const {
    CoopTpCharacter* best = nullptr;
    float bestDistSq = 1.0e12f;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        CoopTpCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending()) {
            continue;
        }
        const glm::vec3 d = ch->GetActorLocation() - from;
        const float distSq = d.x * d.x + d.z * d.z;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = ch;
        }
    }
    return best;
}

void CoopTpGameMode::EnsureAISpawnPlate(leon::Engine& engine) {
    leon::Level& level = engine.GetLevel();
    if (level.FindStaticMeshIndexByTag(kAISpawnPlateTag) != leon::Level::npos) {
        return;
    }

    glm::vec3 pos{-4.0f, matchFloorY_ + 0.12f, 3.0f};
    const int startIdx = findPlayerStartIndex(level, 0);
    if (startIdx >= 0) {
        const leon::PlayerStart& start = level.PlayerStarts()[static_cast<std::size_t>(startIdx)];
        pos = start.transform.position + glm::vec3{-3.5f, 0.12f, 2.5f};
        pos.y = matchFloorY_ + 0.12f;
    }

    leon::StaticMeshComponent plate{};
    plate.tag = kAISpawnPlateTag;
    plate.mesh = engine.GetResources().GetCubeMesh();
    plate.collisionEnabled = true;
    plate.simulatePhysics = false;
    plate.editorClass = "Cube";
    plate.materialOverride = true;
    plate.material.albedo = {0.95f, 0.28f, 0.08f};
    plate.material.roughness = 0.55f;
    plate.transform.position = pos;
    plate.transform.scale = {1.8f, 0.2f, 1.8f};
    level.AddStaticMesh(std::move(plate));

    RegisterBodiesFromLevel(level);
    GetWorld().GetPhysicsScene().SyncFromLevel(level);
    std::cout << "CoopTp: AI spawn plate (NavBlocker) at (" << pos.x << ", " << pos.y << ", "
              << pos.z << ")\n";
}

bool CoopTpGameMode::IsCharacterOnAISpawnPlate(const CoopTpCharacter& character,
                                               const leon::Level& level) const {
    const std::size_t idx = level.FindStaticMeshIndexByTag(kAISpawnPlateTag);
    if (idx == leon::Level::npos) {
        return false;
    }
    const leon::StaticMeshComponent& plate = level.StaticMeshes()[idx];
    float hx = 0.5f;
    float hy = 0.5f;
    float hz = 0.5f;
    leon::HalfExtentsFromScale(plate.transform.scale, hx, hy, hz);
    const glm::vec3& center = plate.transform.position;
    const glm::vec3& feet = character.GetActorLocation();
    const float radius = character.GetCapsule().radius;
    if (!leon::XzDiscOverlapsAabb(feet.x, feet.z, radius, center.x, center.z, hx, hz, 0.08f)) {
        return false;
    }
    const float topY = center.y + hy;
    return feet.y >= (topY - 0.45f) && feet.y <= (topY + 0.75f);
}

void CoopTpGameMode::TickAISpawnPlate(leon::Engine& engine) {
    if (!bAISpawnPlateArmed_ || engine.GetGameInstance().IsClient()) {
        return;
    }
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        CoopTpCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending()) {
            continue;
        }
        if (!IsCharacterOnAISpawnPlate(*ch, engine.GetLevel())) {
            continue;
        }
        bAISpawnPlateArmed_ = false;
        SpawnAIWave(engine, leon::net::kMaxAiPawns);
        return;
    }
}

void CoopTpGameMode::SpawnAIWave(leon::Engine& engine, int count) {
    // Flow: NavBlocker plate overlap (authority) -> CollectAI spawn transforms
    // (Level::AISpawnPoints when authored, else host-relative offsets) ->
    // SpawnActor Character -> AIController::Possess + MoveToActor.
    // Clients: EnsureClientAIProxy from snapshot.
    if (engine.GetGameInstance().IsClient()) {
        return;
    }
    const int n = std::clamp(count, 0, leon::net::kMaxAiPawns);
    ClearAIPawns(true);

    const leon::Level& level = engine.GetLevel();
    const auto& authoredSpawns = level.AISpawnPoints();

    glm::vec3 base{4.0f, matchFloorY_, 4.0f};
    float baseYaw = 180.0f;
    if (CoopTpCharacter* host = characterAt(0)) {
        base = host->GetActorLocation();
    } else {
        const int startIdx = findPlayerStartIndex(level, 0);
        if (startIdx >= 0) {
            const leon::PlayerStart& start = level.PlayerStarts()[static_cast<std::size_t>(startIdx)];
            base = start.transform.position;
            baseYaw = start.transform.rotationDegrees.y + 180.0f;
        }
    }

    constexpr glm::vec3 kOffsets[leon::net::kMaxAiPawns] = {
        {6.0f, 0.0f, 2.0f},
        {6.0f, 0.0f, -2.5f},
    };

    for (int i = 0; i < n; ++i) {
        auto* enemy = GetWorld().SpawnActor<CoopTpCharacter>();
        if (!enemy->GetMesh().LoadFromCooked(engine, kBotCharacter)) {
            std::cerr << "CoopTp: AI pawn without skeletal mesh\n";
        }
        configurePawnForLevel(*enemy, level);
        leon::CharacterMovement& move = enemy->GetCharacterMovement();
        move.MaxWalkSpeed = 3.2f;

        glm::vec3 location{};
        float yaw = baseYaw;
        if (!authoredSpawns.empty()) {
            const leon::AISpawnPoint& spawn =
                authoredSpawns[static_cast<std::size_t>(i) % authoredSpawns.size()];
            location = spawn.transform.position;
            yaw = spawn.transform.rotationDegrees.y;
        } else {
            location = base + kOffsets[static_cast<std::size_t>(i)];
            location.y = matchFloorY_;
        }
        snapPawnToFloor(*enemy, location);
        enemy->Reset(location, yaw + move.ModelYawOffsetDegrees);
        enemy->SyncTransformToLevel(engine.GetLevel());

        aiPawns_[static_cast<std::size_t>(i)] = enemy;
        leon::AIController& ai = aiControllers_[static_cast<std::size_t>(i)];
        ai.SetNavigationSystem(&GetWorld().GetNavigationSystem());
        ai.SetArriveRadius(1.25f);
        ai.Possess(enemy);
        if (CoopTpCharacter* target = FindNearestPlayerCharacter(location)) {
            ai.MoveToActor(target);
        }
        std::cout << "CoopTp: AI[" << i << "] spawn slot=" << AISnapshotSlot(i) << " at ("
                  << location.x << ", " << location.y << ", " << location.z << ")"
                  << (authoredSpawns.empty() ? " fallback-offset" : " AISpawnPoint")
                  << (ai.HasPath() ? " path=nav" : " path=direct") << '\n';
    }

    engine.AddOnScreenDebugMessage("AI wave: 2 chase bots (NavMesh + replicated)", 5.0f,
                                   {1.0f, 0.55f, 0.35f});
}

CoopTpCharacter* CoopTpGameMode::EnsureClientAIProxy(leon::Engine& engine, int aiIndex) {
    if (aiIndex < 0 || aiIndex >= leon::net::kMaxAiPawns) {
        return nullptr;
    }
    CoopTpCharacter*& slot = aiPawns_[static_cast<std::size_t>(aiIndex)];
    if (slot != nullptr && !slot->IsPendingKillPending()) {
        return slot;
    }
    auto* enemy = GetWorld().SpawnActor<CoopTpCharacter>();
    if (!enemy->GetMesh().LoadFromCooked(engine, kBotCharacter)) {
        std::cerr << "CoopTp: client AI proxy without skeletal mesh\n";
    }
    configurePawnForLevel(*enemy, engine.GetLevel());
    enemy->GetCharacterMovement().MaxWalkSpeed = 3.2f;
    slot = enemy;
    return slot;
}

void CoopTpGameMode::TickAIControllers(float deltaTime) {
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        CoopTpCharacter* enemy = aiPawns_[static_cast<std::size_t>(i)];
        if (enemy == nullptr) {
            continue;
        }
        if (enemy->IsPendingKillPending()) {
            aiControllers_[static_cast<std::size_t>(i)].UnPossess();
            aiPawns_[static_cast<std::size_t>(i)] = nullptr;
            continue;
        }
        if (pauseMenuOpen_) {
            enemy->SetAnimBlendInput(0.0f);
            continue;
        }
        leon::AIController& ai = aiControllers_[static_cast<std::size_t>(i)];
        CoopTpCharacter* target = FindNearestPlayerCharacter(enemy->GetActorLocation());
        const glm::vec3 wish = chaseBehavior_.Tick(ai, target, deltaTime);
        const float speedAlpha =
            enemy->IsFalling() ? 0.0f : std::clamp(glm::length(wish), 0.0f, 1.0f);
        enemy->SetAnimBlendInput(speedAlpha);
    }
}

void CoopTpGameMode::syncSessionAddresses(leon::Engine& engine) {
    CoopGameInstance& session = Session(engine);
    if (session.GetLanAddress().empty()) {
        session.SetLanAddress(leon::net::DetectPrimaryLanIPv4());
    }
    if (session.GetJoinAddress().empty()) {
        session.SetJoinAddress("127.0.0.1");
    }
}

void CoopTpGameMode::FinishClientJoin(leon::Engine& engine, std::uint8_t localSlot,
                                      std::string_view hostMapName) {
    CoopGameInstance& session = Session(engine);
    session.SetLocalPlayerId(localSlot);
    localSlot_ = static_cast<int>(localSlot);
    if (!ClientTravelToHostMap(engine, hostMapName)) {
        session.SetClientWelcomed(false);
        return;
    }
    session.SetClientWelcomed(true);
    session.SetMatchMapName(GetMapName());
    spawnAllForClient(engine, localSlot);
    // Travel already applied mid-Tick -- skip soft OnEnter respawn.
    session.SetSkipNextSoftEnter(true);
    engine.AddOnScreenDebugMessage("Joined as slot " + std::to_string(localSlot) + " @ '" +
                                       GetMapName() + "'",
                                   4.0f, {0.4f, 1.0f, 0.6f});
}

void CoopTpGameMode::spawnRemotePlayer(leon::Engine& engine, int playerSlot) {
    LoginPlayer(playerSlot, false);
    engine.AddOnScreenDebugMessage("Player slot " + std::to_string(playerSlot) + " joined", 3.0f,
                                   {0.4f, 1.0f, 0.5f});
}

void CoopTpGameMode::setupNetCallbacks(leon::Engine& engine) {
    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket([this, &engine](int peerSlot, const std::uint8_t* data, std::size_t size) {
        handlePacket(engine, peerSlot, data, size);
    });
    net.SetOnPeerConnected([this, &engine](int peerSlot) {
        if (engine.GetGameInstance().IsClient()) {
            leon::net::HelloMsg hello{};
            engine.GetGameInstance().GetNetDriver().SendToPeer(&hello, sizeof(hello), true);
            return;
        }
        if (engine.GetGameInstance().IsListenServer()) {
            spawnRemotePlayer(engine, 1);
            // Late joiner: Welcome (on Hello) carries map; also push Travel so Lobby clients move.
            NotifyClientsServerTravel(engine);
            return;
        }
        if (engine.GetGameInstance().IsDedicatedServer()) {
            spawnRemotePlayer(engine, peerSlot);
            NotifyClientsServerTravel(engine);
        }
    });
    net.SetOnPeerDisconnected([this, &engine](int peerSlot) {
        if (engine.GetGameInstance().IsListenServer()) {
            Logout(players_[1]);
            remoteInputSeq_[1] = 0;
            engine.AddOnScreenDebugMessage("Client disconnected", 3.0f, {1.0f, 0.6f, 0.3f});
            return;
        }
        if (engine.GetGameInstance().IsDedicatedServer()) {
            const int playerSlot = peerSlot;
            if (playerSlot >= 0 && playerSlot < leon::net::kMaxPlayers) {
                Logout(players_[static_cast<std::size_t>(playerSlot)]);
                remoteInputSeq_[static_cast<std::size_t>(playerSlot)] = 0;
            }
            engine.AddOnScreenDebugMessage("Client slot " + std::to_string(peerSlot) + " left",
                                           3.0f, {1.0f, 0.6f, 0.3f});
            return;
        }
        EndMatch();
        Session(engine).ResetMatchTravelState();
        engine.AddOnScreenDebugMessage("Disconnected from server", 3.0f, {1.0f, 0.4f, 0.3f});
        (void)ClientTravel(engine, CoopGameInstance::kMainMenuMap, levelPath_);
    });
}

void CoopTpGameMode::OnEnter(leon::Engine& engine, const std::string& levelPath) {
    engine_ = &engine;
    refreshLevelIdentity(engine, levelPath);
    // Pack GameModes own travel -- LevelDirector browser would desync peers.
    engine.GetGameInstance().SetLevelBrowserVisible(false);

    // Flow: LevelDirector / Travel reload while a net session is live -> soft respawn, keep
    // sockets.
    if (applyingTravel_) {
        return;
    }
    if (Session(engine).ConsumeSkipNextSoftEnter()) {
        engine.GetGameInstance().NotifyLevelOpened();
        return;
    }
    if (GetGameState().HasMatchStarted() &&
        (engine.GetGameInstance().IsNetHost() ||
         (engine.GetGameInstance().IsClient() && Session(engine).IsClientWelcomed()))) {
        OnTravelFinished(engine);
        engine.GetGameInstance().NotifyLevelOpened();
        return;
    }

    logoutAllPlayers();
    ClearAIPawns(false);
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        pawnTargets_[static_cast<std::size_t>(i)] = {};
        remoteInputSeq_[static_cast<std::size_t>(i)] = 0;
    }
    bodyTargets_.clear();
    GetWorld().Clear();
    aiPawns_.fill(nullptr);
    GetGameState().Reset();
    hostKeyWasDown_ = false;
    dedicatedKeyWasDown_ = false;
    clientKeyWasDown_ = false;
    localhostKeyWasDown_ = false;
    escapeWasDown_ = false;
    pauseMenuOpen_ = false;

    // Match: Jolt rigid Step + body traces; CMC side resolve / floor overlays stay Arcade.
    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    engine.GetAudioDevice().StopMusic();

    // Resume slot / welcome from GameInstance (survives GameMode switches).
    CoopGameInstance& session = Session(engine);
    syncSessionAddresses(engine);
    localSlot_ = static_cast<int>(session.GetLocalPlayerId());
    if (!GetMapName().empty()) {
        session.SetMatchMapName(GetMapName());
    }

    RegisterBodiesFromLevel(engine.GetLevel());
    setupNetCallbacks(engine);

    // Flow: CLI --dedicated (skip menu) OR resume roles opened in MainMenu / Lobby.
    if (engine.GetGameInstance().HasPendingDedicatedStart()) {
        tryDedicated(engine);
    } else if (engine.GetGameInstance().IsDedicatedServer()) {
        beginDedicatedMatch(engine);
        const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
        for (int peer = 0; peer < peers && peer < leon::net::kMaxPlayers; ++peer) {
            spawnRemotePlayer(engine, peer);
        }
    } else if (engine.GetGameInstance().IsListenServer()) {
        spawnAllForListenServer(engine);
        if (engine.GetGameInstance().GetNetDriver().HasPeer()) {
            spawnRemotePlayer(engine, 1);
        }
    } else if (engine.GetGameInstance().IsClient()) {
        if (session.IsClientWelcomed()) {
            spawnAllForClient(engine, static_cast<std::uint8_t>(localSlot_));
        } else if (engine.GetGameInstance().GetNetDriver().IsConnected()) {
            leon::net::HelloMsg hello{};
            engine.GetGameInstance().GetNetDriver().SendToPeer(&hello, sizeof(hello), true);
            engine.AddOnScreenDebugMessage("Waiting for host Welcome / level travel...", 4.0f,
                                           {0.55f, 0.85f, 1.0f});
        } else {
            engine.AddOnScreenDebugMessage("Waiting for host Welcome / level travel...", 4.0f,
                                           {0.55f, 0.85f, 1.0f});
        }
    } else {
        beginStandaloneMatch(engine);
    }

    engine.GetGameInstance().NotifyLevelOpened();
    engine.AddOnScreenDebugMessage(
        "Match [" + GetMapName() +
            "] -- stand on ORANGE plate to spawn 2 AI | Esc pause | Tab players",
        7.0f, {0.55f, 0.9f, 1.0f});
    std::cout << "CoopTp: level='" << GetMapName()
              << "' mode=" << static_cast<int>(engine.GetGameInstance().GetNetMode())
              << " slot=" << localSlot_ << '\n';
}

void CoopTpGameMode::OnExit(leon::Engine& engine) {
    EndMatch();
    logoutAllPlayers();
    ClearAIPawns(false);

    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket(nullptr);
    net.SetOnPeerConnected(nullptr);
    net.SetOnPeerDisconnected(nullptr);
    // Keep sockets alive across Menu/Lobby/Arena GameMode switches; Menu closes the session.

    if (pauseMenu_ != nullptr) {
        engine.GetHUD().RemoveWidget(pauseMenu_);
        pauseMenu_ = nullptr;
    }
    if (scoreboard_ != nullptr) {
        engine.GetHUD().RemoveWidget(scoreboard_);
        scoreboard_ = nullptr;
    }
    pauseMenuOpen_ = false;

    GetWorld().Clear();
    aiPawns_.fill(nullptr);
    engine_ = nullptr;
    engine.ClearCenterHudText();
    engine.SetKeyboardOrbitEnabled(true);
    engine.SetOrbitMouseEnabled(true);
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
}

void CoopTpGameMode::beginStandaloneMatch(leon::Engine& engine) {
    ClearAIPawns(false);
    prepareMatchPhysics(engine);
    localSlot_ = 0;
    LoginPlayer(0, true);
    StartMatch();

    if (CoopTpCharacter* ch = characterAt(0)) {
        ch->SpringArm().SnapLagState(ch->GetActorLocation());
        ch->SpringArm().ApplyToCamera(engine.GetCamera(), ch->GetActorLocation(), 0.0f);
    }
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void CoopTpGameMode::beginDedicatedMatch(leon::Engine& engine) {
    ClearAIPawns(false);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
    }
    StartMatch();
    localSlot_ = -1;

    if (!engine.IsHeadless()) {
        engine.GetCamera().SetTarget({0.0f, matchFloorY_ + 0.5f, 0.0f});
        engine.GetCamera().SetDistance(16.0f);
        engine.GetCamera().SetYawPitch(35.0f, -28.0f);
        engine.SetKeyboardOrbitEnabled(true);
        engine.SetOrbitMouseEnabled(true);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
    }
}

void CoopTpGameMode::PostLogin(leon::PlayerController& newPlayer) {
    leon::GameMode::PostLogin(newPlayer);
    leon::PlayerState& ps = newPlayer.GetPlayerState();
    if (!ps.GetPlayerName().empty()) {
        return;
    }
    const int slot = slotOf(newPlayer);
    const bool dedicated = engine_ != nullptr && engine_->GetGameInstance().IsDedicatedServer();
    if (slot == 0 && !dedicated) {
        ps.SetPlayerName("Host");
    } else {
        ps.SetPlayerName("Player " + std::to_string(slot));
    }
}

void CoopTpGameMode::Logout(leon::PlayerController& exiting) {
    const int slot = slotOf(exiting);
    exiting.UnPossess();
    if (slot >= 0 && slot < leon::net::kMaxPlayers) {
        if (CoopTpCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
            GetWorld().DestroyActor(existing);
            characters_[static_cast<std::size_t>(slot)] = nullptr;
        }
    }
    leon::GameMode::Logout(exiting);
}

void CoopTpGameMode::RestartPlayer(leon::PlayerController& newPlayer) {
    if (engine_ == nullptr) {
        return;
    }
    const int slot = slotOf(newPlayer);
    if (slot < 0 || slot >= leon::net::kMaxPlayers) {
        return;
    }

    newPlayer.UnPossess();
    if (CoopTpCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
        GetWorld().DestroyActor(existing);
        characters_[static_cast<std::size_t>(slot)] = nullptr;
    }

    auto* character = GetWorld().SpawnActor<CoopTpCharacter>();
    if (!character->GetMesh().LoadFromCooked(*engine_, kBotCharacter)) {
        std::cerr << "CoopTp: pawn without skeletal mesh (slot " << slot << ")\n";
    }
    configurePawnForLevel(*character, engine_->GetLevel());

    // Flow: PlayerStart -> soft floor clamp -> QuerySupportY snap (needs SyncFromLevel first).
    const int startIdx = findPlayerStartIndex(engine_->GetLevel(), slot);
    glm::vec3 location{static_cast<float>(slot) * 2.0f, matchFloorY_, 0.0f};
    float yaw = slot == 0 ? 0.0f : 180.0f;
    if (startIdx >= 0) {
        const leon::PlayerStart& start =
            engine_->GetLevel().PlayerStarts()[static_cast<std::size_t>(startIdx)];
        location = start.transform.position;
        yaw = start.transform.rotationDegrees.y;
        if (slot > 0 && engine_->GetLevel().PlayerStarts().size() < 2) {
            location.x += static_cast<float>(slot) * 2.0f;
        }
    } else {
        std::cerr << "CoopTp: no PlayerStart for slot " << slot << " -- using fallback at floor\n";
    }

    leon::SpringArmComponent& boom = character->SpringArm();
    boom.BoomYawDegrees += yaw;
    boom.ClampPitch();
    snapPawnToFloor(*character, location);
    character->Reset(location, yaw + character->GetCharacterMovement().ModelYawOffsetDegrees);
    character->SyncTransformToLevel(engine_->GetLevel());
    characters_[static_cast<std::size_t>(slot)] = character;
    newPlayer.Possess(character);
    std::cout << "CoopTp: spawn slot " << slot << " at (" << location.x << ", " << location.y
              << ", " << location.z << ") floorY=" << matchFloorY_ << '\n';
}

void CoopTpGameMode::spawnAllForListenServer(leon::Engine& engine) {
    ClearAIPawns(false);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);

    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
    }

    localSlot_ = 0;
    LoginPlayer(0, true);
    if (CoopTpCharacter* host = characterAt(0)) {
        host->SpringArm().SnapLagState(host->GetActorLocation());
        host->SpringArm().ApplyToCamera(engine.GetCamera(), host->GetActorLocation(), 0.0f);
    }

    StartMatch();
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void CoopTpGameMode::spawnAllForClient(leon::Engine& engine, std::uint8_t localSlot) {
    ClearAIPawns(false);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    localSlot_ = static_cast<int>(localSlot);

    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
        LoginPlayer(slot, slot == localSlot_);
    }

    StartMatch();
    Session(engine).SetClientWelcomed(true);

    if (CoopTpCharacter* local = characterAt(localSlot_)) {
        local->SpringArm().SnapLagState(local->GetActorLocation());
        local->SpringArm().ApplyToCamera(engine.GetCamera(), local->GetActorLocation(), 0.0f);
    }
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void CoopTpGameMode::tryHost(leon::Engine& engine) {
    if (engine.GetGameInstance().IsListenServer()) {
        engine.AddOnScreenDebugMessage("Already listen host", 2.0f, {0.8f, 0.85f, 0.4f});
        return;
    }
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().HostListen(leon::net::kDefaultPort)) {
        engine.AddOnScreenDebugMessage("Listen host failed", 3.0f, {1.0f, 0.3f, 0.3f});
        return;
    }
    setupNetCallbacks(engine);
    spawnAllForListenServer(engine);
    const std::string tip =
        Session(engine).GetLanAddress().empty() ? "127.0.0.1" : Session(engine).GetLanAddress();
    engine.AddOnScreenDebugMessage("Listen host :7777 -- client presses C (join " + tip + ")", 5.0f,
                                   {0.4f, 1.0f, 0.55f});
}

void CoopTpGameMode::tryDedicated(leon::Engine& engine) {
    if (engine.GetGameInstance().IsDedicatedServer()) {
        engine.GetGameInstance().ClearPendingDedicatedStart();
        engine.AddOnScreenDebugMessage("Already dedicated server", 2.0f, {0.8f, 0.85f, 0.4f});
        return;
    }
    const std::uint16_t port = engine.GetGameInstance().PendingDedicatedPort();
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().HostDedicated(port)) {
        engine.AddOnScreenDebugMessage("Dedicated server failed (port in use?)", 4.0f,
                                       {1.0f, 0.3f, 0.3f});
        return;
    }
    engine.GetGameInstance().ClearPendingDedicatedStart();
    setupNetCallbacks(engine);
    beginDedicatedMatch(engine);
    const std::string tip = Session(engine).GetLanAddress().empty()
                                ? std::string("127.0.0.1")
                                : Session(engine).GetLanAddress();
    engine.AddOnScreenDebugMessage("DEDICATED 0.0.0.0:" + std::to_string(port) +
                                       " -- clients C/V join " + tip,
                                   8.0f, {0.4f, 1.0f, 0.55f});
    std::cout << "CoopTp: dedicated server ready on port " << port << " -- clients Join " << tip
              << ':' << port << '\n';
}

void CoopTpGameMode::tryJoin(leon::Engine& engine, const std::string& address) {
    CoopGameInstance& session = Session(engine);
    session.SetJoinAddress(address.empty() ? "127.0.0.1" : address);
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().Join(session.GetJoinAddress(), leon::net::kDefaultPort)) {
        engine.AddOnScreenDebugMessage("Join failed (" + session.GetJoinAddress() + ")", 3.0f,
                                       {1.0f, 0.3f, 0.3f});
        return;
    }
    setupNetCallbacks(engine);
    session.SetClientWelcomed(false);
    logoutAllPlayers();
    ClearAIPawns(false);
    GetWorld().Clear();
    aiPawns_.fill(nullptr);
    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    RegisterBodiesFromLevel(engine.GetLevel());
    EndMatch();
    engine.AddOnScreenDebugMessage("Joining " + session.GetJoinAddress() + ":7777...", 4.0f,
                                   {0.55f, 0.85f, 1.0f});
}

void CoopTpGameMode::handlePacket(leon::Engine& engine, int peerSlot, const std::uint8_t* data,
                                  std::size_t size) {
    if (data == nullptr || size < 1) {
        return;
    }
    const auto type = static_cast<leon::net::ENetMsg>(data[0]);
    leon::GameInstance& gi = engine.GetGameInstance();

    if (type == leon::net::ENetMsg::Hello && gi.IsNetHost() &&
        size >= sizeof(leon::net::HelloMsg)) {
        leon::net::HelloMsg hello{};
        std::memcpy(&hello, data, sizeof(hello));
        if (!leon::net::IsValidHello(hello)) {
            std::cerr << "CoopTp: Hello rejected (bad magic)\n";
            return;
        }
        leon::net::WelcomeMsg welcome{};
        welcome.slot = static_cast<std::uint8_t>(playerSlotFromPeer(peerSlot));
        leon::net::WriteLevelKey(welcome.levelKey, GetMapName());
        gi.GetNetDriver().SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
        std::cout << "CoopTp: Welcome slot=" << static_cast<int>(welcome.slot) << " level='"
                  << GetMapName() << "'\n";
        return;
    }

    if (type == leon::net::ENetMsg::Welcome && gi.IsClient() &&
        size >= sizeof(leon::net::WelcomeMsg)) {
        if (Session(engine).IsClientWelcomed()) {
            return;
        }
        leon::net::WelcomeMsg welcome{};
        std::memcpy(&welcome, data, sizeof(welcome));
        if (welcome.slot >= leon::net::kMaxPlayers) {
            std::cerr << "CoopTp: Welcome rejected (bad slot)\n";
            return;
        }
        FinishClientJoin(engine, welcome.slot, leon::net::ReadLevelKey(welcome.levelKey));
        return;
    }

    if (type == leon::net::ENetMsg::Travel && gi.IsClient() &&
        size >= sizeof(leon::net::TravelMsg)) {
        leon::net::TravelMsg travel{};
        std::memcpy(&travel, data, sizeof(travel));
        const std::string hostLevel = leon::net::ReadLevelKey(travel.levelKey);
        if (hostLevel.empty() || LevelKeysMatch(hostLevel, GetMapName())) {
            return;
        }
        const std::uint8_t slot =
            travel.slot < leon::net::kMaxPlayers ? travel.slot : Session(engine).GetLocalPlayerId();
        FinishClientJoin(engine, slot, hostLevel);
        return;
    }

    if (type == leon::net::ENetMsg::Rpc) {
        leon::net::RpcHeader rpc{};
        const std::uint8_t* payload = nullptr;
        std::uint16_t payloadBytes = 0;
        if (!leon::net::DecodeRpc(data, size, rpc, payload, payloadBytes)) {
            return;
        }
        // Typed RPC surface: packs handle Notify / custom ids (kRpcIdPackBase+).
        (void)engine;
        (void)peerSlot;
        (void)payload;
        (void)payloadBytes;
        return;
    }

    if (type == leon::net::ENetMsg::InputCmd && gi.IsNetHost() &&
        size >= sizeof(leon::net::InputCmdMsg)) {
        leon::net::InputCmdMsg cmd{};
        std::memcpy(&cmd, data, sizeof(cmd));
        leon::net::SanitizeInputCmd(cmd);
        const int playerSlot = playerSlotFromPeer(peerSlot);
        if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
            return;
        }
        std::uint32_t& seq = remoteInputSeq_[static_cast<std::size_t>(playerSlot)];
        if (cmd.seq >= seq) {
            seq = cmd.seq;
            players_[static_cast<std::size_t>(playerSlot)].ApplyRemoteInput(cmd);
        }
        return;
    }

    if (type == leon::net::ENetMsg::Snapshot && gi.IsClient()) {
        applySnapshot(engine, data, size);
    }
}

void CoopTpGameMode::sendSnapshot(leon::Engine& engine) {
    std::array<glm::vec3, leon::net::kMaxPlayers> viewers{};
    int viewerCount = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        CoopTpCharacter* ch = characterAt(i);
        if (ch == nullptr) {
            continue;
        }
        viewers[static_cast<std::size_t>(viewerCount++)] = ch->GetActorLocation();
    }

    leon::net::PawnSnap pawns[leon::net::kMaxSnapshotPawns]{};
    std::uint8_t pawnCount = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        CoopTpCharacter* ch = characterAt(i);
        if (ch == nullptr) {
            continue;
        }
        FillPawnSnap(pawns[pawnCount++], static_cast<std::uint8_t>(i), *ch);
    }
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        CoopTpCharacter* ch = aiPawns_[static_cast<std::size_t>(i)];
        if (ch == nullptr) {
            continue;
        }
        if (viewerCount > 0 &&
            !leon::net::IsPawnRelevantToAnyXZ(viewers.begin(), viewers.begin() + viewerCount,
                                              ch->GetActorLocation())) {
            continue;
        }
        FillPawnSnap(pawns[pawnCount++], static_cast<std::uint8_t>(AISnapshotSlot(i)), *ch);
    }

    leon::net::BodySnap bodies[leon::net::kMaxDynamicBodies]{};
    std::uint8_t bodyCount = 0;
    for (const leon::BodyInstance& body : GetWorld().GetPhysicsScene().Bodies()) {
        if (body.type != leon::EBodyType::Dynamic) {
            continue;
        }
        if (bodyCount >= leon::net::kMaxDynamicBodies) {
            break;
        }
        leon::net::BodySnap& snap = bodies[bodyCount++];
        snap.levelMeshIndex = static_cast<std::uint32_t>(body.levelMeshIndex);
        snap.x = body.position.x;
        snap.y = body.position.y;
        snap.z = body.position.z;
        snap.velX = body.velXZ.x;
        snap.velY = body.velocityY;
        snap.velZ = body.velXZ.y;
    }

    std::vector<std::uint8_t> packet;
    if (!leon::net::EncodeSnapshot(packet, GetGameState().GetReplicatedWorldTimeFrames(), pawns,
                                   pawnCount, bodies, bodyCount)) {
        return;
    }
    engine.GetGameInstance().GetNetDriver().Broadcast(packet.data(), packet.size(), false);
}

void CoopTpGameMode::applySnapshot(leon::Engine& engine, const std::uint8_t* data,
                                   std::size_t size) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }
    leon::net::DecodedSnapshot decoded;
    if (!leon::net::DecodeSnapshot(data, size, decoded)) {
        return;
    }

    GetGameState().SetReplicatedWorldTimeFrames(decoded.tick);

    bool aiSeen[leon::net::kMaxAiPawns]{};
    for (const leon::net::PawnSnap& snap : decoded.pawns) {
        if (snap.slot < leon::net::kMaxPlayers) {
            PawnTarget& target = pawnTargets_[snap.slot];
            target.pos = {snap.x, snap.y, snap.z};
            target.yaw = snap.yaw;
            target.velY = snap.velY;
            target.animBlend = snap.animBlend;
            target.boomYaw = snap.boomYaw;
            target.boomPitch = snap.boomPitch;
            target.grounded = snap.grounded != 0;
            target.valid = true;
            continue;
        }
        const int aiIndex = AIIndexFromSnapshotSlot(static_cast<int>(snap.slot));
        if (aiIndex < 0 || aiIndex >= leon::net::kMaxAiPawns) {
            continue;
        }
        aiSeen[static_cast<std::size_t>(aiIndex)] = true;
        if (EnsureClientAIProxy(engine, aiIndex) == nullptr) {
            continue;
        }
        PawnTarget& target = aiPawnTargets_[static_cast<std::size_t>(aiIndex)];
        target.pos = {snap.x, snap.y, snap.z};
        target.yaw = snap.yaw;
        target.velY = snap.velY;
        target.animBlend = snap.animBlend;
        target.boomYaw = snap.boomYaw;
        target.boomPitch = snap.boomPitch;
        target.grounded = snap.grounded != 0;
        target.valid = true;
    }
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        if (aiSeen[static_cast<std::size_t>(i)]) {
            continue;
        }
        aiPawnTargets_[static_cast<std::size_t>(i)].valid = false;
    }

    bodyTargets_.clear();
    bodyTargets_.reserve(decoded.bodies.size());
    for (const leon::net::BodySnap& snap : decoded.bodies) {
        BodyTarget t{};
        t.levelMeshIndex = snap.levelMeshIndex;
        t.pos = {snap.x, snap.y, snap.z};
        t.vel = {snap.velX, snap.velY, snap.velZ};
        t.valid = true;
        bodyTargets_.push_back(t);
    }
}

void CoopTpGameMode::tickHost(leon::Engine& engine, float deltaTime) {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (characters_[static_cast<std::size_t>(i)] == nullptr) {
            continue;
        }
        // Local pause overlay blocks only local input; remotes / sim keep running.
        if (pauseMenuOpen_ && i == localSlot_) {
            characters_[static_cast<std::size_t>(i)]->SetAnimBlendInput(0.0f);
            continue;
        }
        const glm::vec3 move = players_[static_cast<std::size_t>(i)].TickInput(engine);
        const float speedAlpha = characters_[static_cast<std::size_t>(i)]->IsFalling()
                                     ? 0.0f
                                     : std::clamp(glm::length(move), 0.0f, 1.0f);
        characters_[static_cast<std::size_t>(i)]->SetAnimBlendInput(speedAlpha);
    }

    TickAIControllers(deltaTime);

    leon::WorldGameplayFrameParams frame{};
    frame.deltaTime = deltaTime;
    frame.level = &engine.GetLevel();
    frame.renderer = (engine.IsHeadless() || engine.GetGameInstance().IsDedicatedServer())
                         ? nullptr
                         : &engine.GetRenderer();
    frame.collisionDebugDraw = (!engine.IsHeadless() && engine.IsCollisionDebugEnabled())
                                   ? &engine.GetRenderer().GetDebugOverlay()
                                   : nullptr;
    frame.navMeshDebugDraw = (!engine.IsHeadless() && engine.IsNavMeshDebugEnabled())
                                 ? &engine.GetRenderer().GetDebugOverlay()
                                 : nullptr;
    GetWorld().TickGameplayFrame(frame);
    TickAISpawnPlate(engine);

    if (engine.GetGameInstance().IsListenServer() && localSlot_ >= 0 && !pauseMenuOpen_) {
        players_[static_cast<std::size_t>(localSlot_)].UpdateCamera(engine, deltaTime);
    } else if (engine.GetGameInstance().IsDedicatedServer() && !engine.IsHeadless()) {
        tickDedicatedSpectator(engine, deltaTime);
    }

    GetGameState().IncrementReplicatedWorldTimeFrames();
    if (engine.GetGameInstance().GetNetDriver().HasPeer()) {
        sendSnapshot(engine);
    }
}

void CoopTpGameMode::tickDedicatedSpectator(leon::Engine& /*engine*/, float /*deltaTime*/) {}

void CoopTpGameMode::tickClient(leon::Engine& engine, float deltaTime) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }

    if (!pauseMenuOpen_ && characterAt(localSlot_) != nullptr) {
        players_[static_cast<std::size_t>(localSlot_)].TickInput(engine);
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].ConsumeLocalInputCmd();
        // Jump edges are reliable -- a single lost datagram used to drop ~1/3 of Spaces.
        const bool reliable = cmd.jump != 0;
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), reliable);
    } else if (characterAt(localSlot_) != nullptr) {
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].MakeIdleInputCmd();
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), false);
    }

    const float a = ExpAlpha(14.0f, deltaTime);
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        CoopTpCharacter* ch = characterAt(i);
        const PawnTarget& t = pawnTargets_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !t.valid) {
            continue;
        }
        const glm::vec3 pos = glm::mix(ch->GetActorLocation(), t.pos, a);
        const float yaw = LerpAngleDegrees(ch->GetActorYaw(), t.yaw, a);
        ch->ApplyReplicatedState(pos, yaw, t.velY, t.grounded);
        ch->SetAnimBlendInput(t.animBlend);
        if (i != localSlot_) {
            ch->SpringArm().BoomYawDegrees =
                LerpAngleDegrees(ch->SpringArm().BoomYawDegrees, t.boomYaw, a);
            ch->SpringArm().BoomPitchDegrees =
                glm::mix(ch->SpringArm().BoomPitchDegrees, t.boomPitch, a);
        }
        ch->Tick(deltaTime);
        if (!engine.IsHeadless()) {
            ch->SubmitMeshDraw(engine.GetRenderer());
        }
        ch->SyncTransformToLevel(engine.GetLevel());
    }

    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        CoopTpCharacter* ch = aiPawns_[static_cast<std::size_t>(i)];
        const PawnTarget& t = aiPawnTargets_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !t.valid) {
            continue;
        }
        const glm::vec3 pos = glm::mix(ch->GetActorLocation(), t.pos, a);
        const float yaw = LerpAngleDegrees(ch->GetActorYaw(), t.yaw, a);
        ch->ApplyReplicatedState(pos, yaw, t.velY, t.grounded);
        ch->SetAnimBlendInput(t.animBlend);
        ch->Tick(deltaTime);
        if (!engine.IsHeadless()) {
            ch->SubmitMeshDraw(engine.GetRenderer());
        }
        ch->SyncTransformToLevel(engine.GetLevel());
    }

    auto& meshes = engine.GetLevel().StaticMeshes();
    auto& bodies = GetWorld().GetPhysicsScene().Bodies();
    for (const BodyTarget& t : bodyTargets_) {
        if (!t.valid || t.levelMeshIndex >= meshes.size()) {
            continue;
        }
        meshes[t.levelMeshIndex].transform.position = t.pos;
        for (leon::BodyInstance& body : bodies) {
            if (body.levelMeshIndex == t.levelMeshIndex && body.type == leon::EBodyType::Dynamic) {
                body.position = t.pos;
                body.velXZ = {t.vel.x, t.vel.z};
                body.velocityY = t.vel.y;
                break;
            }
        }
    }

    if (!pauseMenuOpen_) {
        players_[static_cast<std::size_t>(localSlot_)].UpdateCamera(engine, deltaTime);
    }
}

void CoopTpGameMode::updateNetHud(leon::Engine& engine) {
    if (engine.IsHeadless() || pauseMenuOpen_) {
        return;
    }
    if (scoreboard_ != nullptr && scoreboard_->IsVisible()) {
        engine.ClearCenterHudText();
        return;
    }
    const leon::ENetMode mode = engine.GetGameInstance().GetNetMode();
    const std::string tip = Session(engine).GetJoinAddress();
    const std::uint16_t port = engine.GetGameInstance().PendingDedicatedPort();
    if (mode == leon::ENetMode::DedicatedServer) {
        engine.SetCenterHudText(
            "DEDICATED " + tip + ":" + std::to_string(port) + " -- clients " +
            std::to_string(engine.GetGameInstance().GetNetDriver().PeerCount()) + "/" +
            std::to_string(engine.GetGameInstance().GetNetDriver().MaxClients()) + "  [" +
            GetMapName() + "]  Esc=pause  Tab=players");
    } else if (mode == leon::ENetMode::ListenServer) {
        if (engine.GetGameInstance().GetNetDriver().HasPeer()) {
            engine.SetCenterHudText("LISTEN HOST -- 2P  [" + GetMapName() +
                                    "]  Esc=pause  Tab=players");
        } else {
            engine.SetCenterHudText("LISTEN HOST :7777 -- waiting  [" + GetMapName() +
                                    "]  Esc=pause  Tab=players");
        }
    } else if (mode == leon::ENetMode::Client) {
        if (Session(engine).IsClientWelcomed()) {
            engine.SetCenterHudText("CLIENT slot " + std::to_string(localSlot_) + " @ " + tip +
                                    " [" + GetMapName() + "]  Esc=pause  Tab=players");
        } else {
            engine.SetCenterHudText("CLIENT joining " + tip + ":7777...");
        }
    } else {
        engine.SetCenterHudText("STANDALONE [" + GetMapName() +
                                "] -- Esc pause | Tab players | H Listen | C Join " + tip +
                                " | V localhost | F9");
    }
}

std::string CoopTpGameMode::buildScoreboardText(const leon::Engine& engine) const {
    // Unreal-ish scoreboard: GameState = match/map/count; PlayerState = per-row id/name.
    const leon::GameState& gs = GetGameState();
    const leon::ENetMode mode = engine.GetGameInstance().GetNetMode();
    const std::string& mapName = !gs.GetMapName().empty() ? gs.GetMapName() : GetMapName();

    std::string text = "PLAYERS";
    if (mode == leon::ENetMode::ListenServer) {
        text += "  (Listen Host)";
    } else if (mode == leon::ENetMode::DedicatedServer) {
        text += "  (Dedicated)";
    } else if (mode == leon::ENetMode::Client) {
        text += "  (Client)";
    } else {
        text += "  (Standalone)";
    }
    text += "\n[" + mapName + "]";

    const auto& playerArray = gs.GetPlayerArray();
    if (playerArray.empty()) {
        if (mode == leon::ENetMode::Client && !Session(engine).IsClientWelcomed()) {
            text += "\n  Connecting...";
        } else {
            text += "\n  (none connected)";
        }
    } else {
        for (const leon::PlayerState* ps : playerArray) {
            if (ps == nullptr) {
                continue;
            }
            std::string name = ps->GetPlayerName();
            if (name.empty()) {
                const bool dedicated = mode == leon::ENetMode::DedicatedServer;
                name = (ps->GetPlayerId() == 0 && !dedicated)
                           ? "Host"
                           : ("Player " + std::to_string(ps->GetPlayerId()));
            }
            text += "\n  " + std::to_string(ps->GetPlayerId()) + "  " + name;
            if (mode != leon::ENetMode::DedicatedServer && ps->GetPlayerId() == localSlot_) {
                text += "  (You)";
            }
        }
        text += "\n\n" + std::to_string(gs.GetNumPlayers()) + "/" +
                std::to_string(leon::net::kMaxPlayers) + " in match";
    }
    text += "\n(hold Tab)";
    return text;
}

void CoopTpGameMode::updateScoreboard(leon::Engine& engine, bool tabHeld) {
    if (engine.IsHeadless()) {
        return;
    }
    const bool show = tabHeld && !pauseMenuOpen_;
    if (!show) {
        if (scoreboard_ != nullptr) {
            scoreboard_->SetVisibility(false);
        }
        return;
    }
    if (scoreboard_ == nullptr) {
        scoreboard_ = engine.GetHUD().AddWidget<leon::TextBlockWidget>();
        scoreboard_->SetCenteredOnScreen(true);
        scoreboard_->SetColor({0.95f, 0.92f, 0.75f});
    }
    scoreboard_->SetVisibility(true);
    scoreboard_->SetText(buildScoreboardText(engine));
}

void CoopTpGameMode::openPauseMenu(leon::Engine& engine) {
    pauseMenuOpen_ = true;
    engine.ClearCenterHudText();
    if (scoreboard_ != nullptr) {
        scoreboard_->SetVisibility(false);
    }
    if (pauseMenu_ == nullptr) {
        pauseMenu_ = engine.GetHUD().AddWidget<leon::VerticalBoxWidget>();
    }
    pauseMenu_->SetVisibility(true);
    pauseMenu_->SetTitle("PAUSE");
    pauseMenu_->SetHint("Arrows / click  |  Enter  |  Esc / Backspace Resume");
    pauseMenu_->ClearChildren();
    pauseMenu_->AddButton("continue", "Resume");
    pauseMenu_->AddButton("menu", "Quit to Main Menu");
    pauseMenu_->SetSelectedIndex(0);
    pauseMenu_->ResetEdges();
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
}

void CoopTpGameMode::closePauseMenu(leon::Engine& engine) {
    pauseMenuOpen_ = false;
    if (pauseMenu_ != nullptr) {
        pauseMenu_->SetVisibility(false);
    }
    engine.ClearCenterHudText();
    if (!engine.GetGameInstance().IsDedicatedServer()) {
        engine.SetPlayMouseLookActive(true);
        engine.SetCursorCaptured(true);
    }
}

void CoopTpGameMode::returnToMenu(leon::Engine& engine) {
    pauseMenuOpen_ = false;
    if (pauseMenu_ != nullptr) {
        engine.GetHUD().RemoveWidget(pauseMenu_);
        pauseMenu_ = nullptr;
    }
    if (scoreboard_ != nullptr) {
        engine.GetHUD().RemoveWidget(scoreboard_);
        scoreboard_ = nullptr;
    }
    engine.GetGameInstance().CloseNetSession();
    Session(engine).ResetSession();
    EndMatch();
    if (engine.GetGameInstance().IsClient()) {
        (void)ClientTravel(engine, CoopGameInstance::kMainMenuMap, levelPath_);
    } else {
        (void)ServerTravel(engine, CoopGameInstance::kMainMenuMap, levelPath_);
    }
}

void CoopTpGameMode::Tick(leon::Engine& engine, float deltaTime) {
    GetGameState().Tick(deltaTime);

    if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetPlayInputWindow();
        const bool pauseToggleDown = window.IsKeyPressed(GLFW_KEY_ESCAPE) ||
                                     window.IsKeyPressed(GLFW_KEY_BACKSPACE) ||
                                     window.IsKeyPressed(GLFW_KEY_DELETE);
        if (pauseToggleDown && !escapeWasDown_) {
            if (pauseMenuOpen_) {
                closePauseMenu(engine);
            } else {
                openPauseMenu(engine);
            }
        }
        escapeWasDown_ = pauseToggleDown;
    }

    if (pauseMenuOpen_ && !engine.IsHeadless() && pauseMenu_ != nullptr) {
        const std::string id = pauseMenu_->TickInput(engine.GetPlayInputWindow(),
                                                     engine.IsCursorCaptured(), deltaTime);
        if (id == "continue") {
            engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Back);
            closePauseMenu(engine);
        } else if (id == "menu") {
            engine.GetAudioDevice().PlayUiSound(leon::EUiSound::Confirm);
            returnToMenu(engine);
            return;
        }
    }

    // Net role keys only while standalone and not paused -- WASD must not start a server.
    const bool standalone = engine.GetGameInstance().GetNetMode() == leon::ENetMode::Standalone;
    if (!engine.IsHeadless() && standalone && !pauseMenuOpen_) {
        leon::Window& window = engine.GetWindow();
        const bool hostDown = window.IsKeyPressed(GLFW_KEY_H);
        if (hostDown && !hostKeyWasDown_) {
            tryHost(engine);
        }
        hostKeyWasDown_ = hostDown;

        const bool dedicatedDown = window.IsKeyPressed(GLFW_KEY_F9);
        if (dedicatedDown && !dedicatedKeyWasDown_) {
            tryDedicated(engine);
        }
        dedicatedKeyWasDown_ = dedicatedDown;

        const bool clientDown = window.IsKeyPressed(GLFW_KEY_C);
        if (clientDown && !clientKeyWasDown_) {
            const std::string addr = Session(engine).GetLanAddress().empty()
                                         ? "127.0.0.1"
                                         : Session(engine).GetLanAddress();
            tryJoin(engine, addr);
        }
        clientKeyWasDown_ = clientDown;

        const bool localhostDown = window.IsKeyPressed(GLFW_KEY_V);
        if (localhostDown && !localhostKeyWasDown_) {
            tryJoin(engine, "127.0.0.1");
        }
        localhostKeyWasDown_ = localhostDown;
    } else if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetWindow();
        hostKeyWasDown_ = window.IsKeyPressed(GLFW_KEY_H);
        dedicatedKeyWasDown_ = window.IsKeyPressed(GLFW_KEY_F9);
        clientKeyWasDown_ = window.IsKeyPressed(GLFW_KEY_C);
        localhostKeyWasDown_ = window.IsKeyPressed(GLFW_KEY_V);
    }

    if (!engine.IsHeadless()) {
        const bool tabHeld = engine.GetPlayInputWindow().IsKeyPressed(GLFW_KEY_TAB);
        updateScoreboard(engine, tabHeld);
    }

    engine.GetGameInstance().GetNetDriver().Poll();
    if (pauseMenuOpen_) {
        engine.ClearCenterHudText();
    } else {
        updateNetHud(engine);
    }

    if (!GetGameState().HasMatchStarted() && !engine.GetGameInstance().IsClient()) {
        return;
    }

    if (engine.GetGameInstance().IsNetHost()) {
        tickHost(engine, deltaTime);
    } else if (engine.GetGameInstance().IsClient()) {
        tickClient(engine, deltaTime);
    } else {
        CoopTpCharacter* ch = characterAt(0);
        if (ch == nullptr || ch->IsPendingKillPending()) {
            return;
        }
        glm::vec3 move{};
        if (!pauseMenuOpen_) {
            move = players_[0].TickInput(engine);
        }
        const float speedAlpha = ch->IsFalling() ? 0.0f : std::clamp(glm::length(move), 0.0f, 1.0f);
        ch->SetAnimBlendInput(speedAlpha);

        TickAIControllers(deltaTime);

        leon::WorldGameplayFrameParams frame{};
        frame.deltaTime = deltaTime;
        frame.level = &engine.GetLevel();
        frame.renderer = engine.IsHeadless() ? nullptr : &engine.GetRenderer();
        frame.collisionDebugDraw = (!engine.IsHeadless() && engine.IsCollisionDebugEnabled())
                                       ? &engine.GetRenderer().GetDebugOverlay()
                                       : nullptr;
        frame.navMeshDebugDraw = (!engine.IsHeadless() && engine.IsNavMeshDebugEnabled())
                                     ? &engine.GetRenderer().GetDebugOverlay()
                                     : nullptr;
        GetWorld().TickGameplayFrame(frame);
        TickAISpawnPlate(engine);
        if (!pauseMenuOpen_) {
            players_[0].UpdateCamera(engine, deltaTime);
        }
    }
}

} // namespace game
