#include "ZombiesGameMode.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <leon/core/Ascii.h>
#include <leon/core/Window.h>
#include <leon/debug/DebugDraw.h>
#include <leon/Engine.h>
#include <leon/level/Level.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetUtil.h>
#include <leon/net/RootReplication.h>
#include <leon/net/SnapshotCodec.h>
#include <leon/physics/BodyInstance.h>
#include <leon/physics/CollisionQuery.h>
#include <leon/physics/CollisionShape.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/StaticMesh.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ZombieCharacter.h"
#include "ZombiesCharacter.h"
#include "ZombiesGameInstance.h"
#include "ZombiesInteract.h"
#include "ZombiesPlayerState.h"
#include "ZombiesWeapon.h"

namespace game {
namespace {

[[nodiscard]] ZombiesGameInstance& Session(leon::Engine& engine) {
    return ZombiesGameInstance::Get(engine.GetGameInstance());
}

[[nodiscard]] const ZombiesGameInstance& Session(const leon::Engine& engine) {
    return ZombiesGameInstance::Get(engine.GetGameInstance());
}

constexpr float kIntermissionSeconds = 5.0f;
constexpr float kZombieWalkSpeed = 2.2f;
constexpr float kHitscanDamage = 28.0f;
constexpr float kHitscanRange = 90.0f;
/// Auto-fire cadence while LMB held (~500 RPM).
constexpr float kFireIntervalSeconds = 0.12f;
constexpr float kShotFxLifetime = 0.18f;
constexpr float kContactDamage = 15.0f;
constexpr float kContactCooldown = 0.8f;
constexpr float kContactReach = 0.95f;
constexpr float kKillScore = 100.0f;
constexpr float kStartingPoints = 500.0f;
constexpr float kInteractRange = 2.4f;
constexpr const char* kZombieSpawnTag = "ZombieSpawn";

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

void YawAxes(float yawDegrees, glm::vec3& outForward, glm::vec3& outRight) {
    const float rad = glm::radians(yawDegrees);
    outForward = glm::normalize(glm::vec3{std::sin(rad), 0.0f, std::cos(rad)});
    outRight = glm::normalize(glm::vec3{outForward.z, 0.0f, -outForward.x});
}

void DrawOrientedBox(leon::DebugDraw& draw, const glm::vec3& center, const glm::vec3& half,
                     const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up,
                     const glm::vec3& color) {
    const glm::vec3 c[8] = {
        center + right * half.x + up * half.y + forward * half.z,
        center - right * half.x + up * half.y + forward * half.z,
        center - right * half.x + up * half.y - forward * half.z,
        center + right * half.x + up * half.y - forward * half.z,
        center + right * half.x - up * half.y + forward * half.z,
        center - right * half.x - up * half.y + forward * half.z,
        center - right * half.x - up * half.y - forward * half.z,
        center + right * half.x - up * half.y - forward * half.z,
    };
    static constexpr int kEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                          {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : kEdges) {
        draw.AddLine(c[e[0]], c[e[1]], color);
    }
}

void DrawWireSphere(leon::DebugDraw& draw, const glm::vec3& center, float radius,
                    const glm::vec3& color, int segments = 10) {
    const float n = static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / n) * 6.2831853f;
        const float a1 = (static_cast<float>(i + 1) / n) * 6.2831853f;
        const float c0 = std::cos(a0) * radius;
        const float s0 = std::sin(a0) * radius;
        const float c1 = std::cos(a1) * radius;
        const float s1 = std::sin(a1) * radius;
        draw.AddLine(center + glm::vec3{c0, 0, s0}, center + glm::vec3{c1, 0, s1}, color);
        draw.AddLine(center + glm::vec3{c0, s0, 0}, center + glm::vec3{c1, s1, 0}, color);
        draw.AddLine(center + glm::vec3{0, c0, s0}, center + glm::vec3{0, c1, s1}, color);
    }
}

/// Soldier: blocky torso/legs/head + rifle. Oriented by actor yaw.
void DrawPlayerPrimitive(leon::DebugDraw& draw, const glm::vec3& feet, float yawDegrees,
                         const glm::vec3& accent) {
    glm::vec3 forward{};
    glm::vec3 right{};
    YawAxes(yawDegrees, forward, right);
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const glm::vec3 body = accent;
    const glm::vec3 dark = accent * 0.55f;
    const glm::vec3 metal{0.75f, 0.75f, 0.7f};

    // Legs
    DrawOrientedBox(draw, feet + up * 0.35f - right * 0.12f, {0.08f, 0.35f, 0.08f}, forward, right,
                    up, dark);
    DrawOrientedBox(draw, feet + up * 0.35f + right * 0.12f, {0.08f, 0.35f, 0.08f}, forward, right,
                    up, dark);
    // Torso / pack
    DrawOrientedBox(draw, feet + up * 1.05f, {0.22f, 0.35f, 0.14f}, forward, right, up, body);
    DrawOrientedBox(draw, feet + up * 1.05f - forward * 0.16f, {0.18f, 0.28f, 0.08f}, forward,
                    right, up, dark);
    // Head + helmet brim
    DrawWireSphere(draw, feet + up * 1.55f, 0.16f, body, 8);
    DrawOrientedBox(draw, feet + up * 1.62f + forward * 0.02f, {0.18f, 0.06f, 0.18f}, forward,
                    right, up, dark);
    // Arms
    DrawOrientedBox(draw, feet + up * 1.15f - right * 0.32f, {0.07f, 0.28f, 0.07f}, forward, right,
                    up, body);
    DrawOrientedBox(draw, feet + up * 1.15f + right * 0.32f, {0.07f, 0.28f, 0.07f}, forward, right,
                    up, body);
    // Rifle held across chest toward forward-right
    const glm::vec3 grip = feet + up * 1.1f + right * 0.18f + forward * 0.1f;
    DrawOrientedBox(draw, grip + forward * 0.35f, {0.04f, 0.04f, 0.42f}, forward, right, up, metal);
    DrawOrientedBox(draw, grip + forward * 0.05f + up * 0.06f, {0.05f, 0.08f, 0.08f}, forward,
                    right, up, dark);
}

/// Hunched walker: longer arms, lopsided head, rotten palette.
void DrawZombiePrimitive(leon::DebugDraw& draw, const glm::vec3& feet, float yawDegrees) {
    glm::vec3 forward{};
    glm::vec3 right{};
    YawAxes(yawDegrees, forward, right);
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    constexpr glm::vec3 kFlesh{0.35f, 0.75f, 0.28f};
    constexpr glm::vec3 kRot{0.2f, 0.45f, 0.15f};
    constexpr glm::vec3 kGore{0.7f, 0.15f, 0.1f};

    // Hunched pelvis / torso lean forward
    DrawOrientedBox(draw, feet + up * 0.32f - right * 0.11f, {0.09f, 0.32f, 0.09f}, forward, right,
                    up, kRot);
    DrawOrientedBox(draw, feet + up * 0.32f + right * 0.11f, {0.09f, 0.28f, 0.09f}, forward, right,
                    up, kRot);
    const glm::vec3 torso = feet + up * 0.95f + forward * 0.12f;
    DrawOrientedBox(draw, torso, {0.2f, 0.32f, 0.16f}, forward, right, up, kFlesh);
    DrawOrientedBox(draw, torso + up * 0.05f - right * 0.05f, {0.08f, 0.12f, 0.06f}, forward, right,
                    up, kGore);
    // Lopsided skull
    DrawWireSphere(draw, feet + up * 1.42f + forward * 0.22f + right * 0.04f, 0.17f, kFlesh, 7);
    DrawOrientedBox(draw, feet + up * 1.48f + forward * 0.28f, {0.12f, 0.05f, 0.1f}, forward, right,
                    up, kRot);
    // Long dangling arms (reach pose)
    DrawOrientedBox(draw, feet + up * 0.85f - right * 0.38f + forward * 0.25f,
                    {0.06f, 0.45f, 0.06f}, forward, right, up, kFlesh);
    DrawOrientedBox(draw, feet + up * 0.7f + right * 0.36f + forward * 0.35f, {0.06f, 0.5f, 0.06f},
                    forward, right, up, kFlesh);
    DrawOrientedBox(draw, feet + up * 0.35f - right * 0.4f + forward * 0.45f, {0.08f, 0.06f, 0.1f},
                    forward, right, up, kGore);
    DrawOrientedBox(draw, feet + up * 0.28f + right * 0.38f + forward * 0.55f, {0.08f, 0.06f, 0.1f},
                    forward, right, up, kGore);
}

/// Local FP viewmodel: short arms + gun along boom aim (not full body).
void DrawLocalFpViewmodel(leon::DebugDraw& draw, const ZombiesCharacter& ch) {
    const leon::SpringArmComponent& boom = ch.SpringArm();
    const glm::vec3 eye = boom.GetTargetLocation(ch.GetActorLocation());
    const glm::vec3 aim =
        -leon::SpringArmComponent::GetBoomDirection(boom.BoomYawDegrees, boom.BoomPitchDegrees);
    glm::vec3 right = glm::normalize(glm::cross(aim, glm::vec3{0.0f, 1.0f, 0.0f}));
    if (glm::dot(right, right) < 1.0e-6f) {
        right = glm::vec3{1.0f, 0.0f, 0.0f};
    }
    const glm::vec3 up = glm::normalize(glm::cross(right, aim));
    constexpr glm::vec3 kSkin{0.55f, 0.75f, 0.95f};
    constexpr glm::vec3 kGun{0.7f, 0.7f, 0.65f};
    const glm::vec3 grip = eye + aim * 0.35f + right * 0.18f - up * 0.18f;
    DrawOrientedBox(draw, grip, {0.04f, 0.04f, 0.12f}, aim, right, up, kSkin);
    DrawOrientedBox(draw, grip + aim * 0.28f, {0.035f, 0.035f, 0.32f}, aim, right, up, kGun);
    DrawOrientedBox(draw, grip + aim * 0.05f - up * 0.05f, {0.05f, 0.07f, 0.06f}, aim, right, up,
                    kGun * 0.7f);
}

/// Segment vs vertical capsule: returns true and hitT in [0,1] along start→end.
[[nodiscard]] bool SegmentHitsCapsule(const glm::vec3& start, const glm::vec3& end,
                                      const glm::vec3& feet, const leon::CapsuleShape& capsule,
                                      float& outHitT) {
    const glm::vec3 rd = end - start;
    const float rdLen = glm::length(rd);
    if (rdLen < 1.0e-6f) {
        return false;
    }
    const glm::vec3 ro = start;
    const glm::vec3 dir = rd / rdLen;

    const float radius = capsule.radius;
    const float h = capsule.height;
    const glm::vec3 pa = feet + glm::vec3{0.0f, radius, 0.0f};
    const glm::vec3 pb = feet + glm::vec3{0.0f, std::max(h - radius, radius), 0.0f};
    const glm::vec3 ba = pb - pa;
    const glm::vec3 oa = ro - pa;
    const float baba = glm::dot(ba, ba);
    const float bard = glm::dot(ba, dir);
    const float baoa = glm::dot(ba, oa);
    const float rdoa = glm::dot(dir, oa);
    const float oaoa = glm::dot(oa, oa);
    const float A = baba - bard * bard;
    const float B = baba * rdoa - baoa * bard;
    const float C = baba * oaoa - baoa * baoa - radius * radius * baba;
    const float disc = B * B - A * C;

    float tRay = -1.0f;
    if (disc >= 0.0f && std::abs(A) > 1.0e-8f) {
        const float t = (-B - std::sqrt(disc)) / A;
        const float y = baoa + t * bard;
        if (y > 0.0f && y < baba) {
            tRay = t;
        } else {
            const glm::vec3 oc = (y <= 0.0f) ? oa : (ro - pb);
            const float b2 = glm::dot(dir, oc);
            const float c2 = glm::dot(oc, oc) - radius * radius;
            const float h2 = b2 * b2 - c2;
            if (h2 > 0.0f) {
                tRay = -b2 - std::sqrt(h2);
            }
        }
    }
    if (tRay < 0.0f || tRay > rdLen) {
        return false;
    }
    outHitT = tRay / rdLen;
    return true;
}

void FillPawnSnap(leon::net::PawnSnap& snap, std::uint8_t slot, const ZombiesCharacter& ch) {
    snap = leon::net::CaptureCharacterRoot(slot, ch, ch.SpringArm().BoomYawDegrees,
                                           ch.SpringArm().BoomPitchDegrees);
    snap.health = ch.GetHealth();
    snap.flags = ch.IsAlive() ? leon::net::kPawnSnapAlive : 0;
    if (slot < static_cast<std::uint8_t>(leon::net::kMaxPlayers)) {
        leon::net::SetPawnUserAmmo(
            snap, static_cast<std::uint8_t>(std::clamp(ch.GetAmmoInMag(), 0, 255)),
            static_cast<std::uint16_t>(std::clamp(ch.GetAmmoReserve(), 0, 65535)));
    } else {
        leon::net::SetPawnUserAmmo(snap, 0, 0);
    }
}

[[nodiscard]] ZombiesPlayerState* AsZombiesPS(leon::PlayerState* ps) {
    return dynamic_cast<ZombiesPlayerState*>(ps);
}

[[nodiscard]] const ZombiesPlayerState* AsZombiesPS(const leon::PlayerState* ps) {
    return dynamic_cast<const ZombiesPlayerState*>(ps);
}

} // namespace

void ZombiesGameMode::InitGameState() {
    // Default GameState already constructed via SetGameState<ZombiesGameState> in ctor.
}

ZombiesGameState& ZombiesGameMode::MatchGameState() {
    ZombiesGameState* gs = GetGameState<ZombiesGameState>();
    return gs != nullptr ? *gs : static_cast<ZombiesGameState&>(GetGameState());
}

const ZombiesGameState& ZombiesGameMode::MatchGameState() const {
    const ZombiesGameState* gs = GetGameState<ZombiesGameState>();
    return gs != nullptr ? *gs : static_cast<const ZombiesGameState&>(GetGameState());
}

bool ZombiesGameMode::Matches(const leon::LevelEntry& /*entry*/,
                              const std::string& gameModeId) const {
    return gameModeId == Id() || gameModeId == "Zombies" || gameModeId == "Nacht" ||
           gameModeId == "Town";
}

int ZombiesGameMode::findPlayerStartIndex(const leon::Level& level, int slot) const {
    const auto& starts = level.PlayerStarts();
    if (starts.empty()) {
        return -1;
    }
    return std::clamp(slot, 0, static_cast<int>(starts.size()) - 1);
}

ZombiesCharacter* ZombiesGameMode::characterAt(int slot) const {
    if (slot < 0 || slot >= leon::net::kMaxPlayers) {
        return nullptr;
    }
    return characters_[static_cast<std::size_t>(slot)];
}

int ZombiesGameMode::slotOf(const leon::PlayerController& pc) const {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (&players_[static_cast<std::size_t>(i)] == &pc) {
            return i;
        }
    }
    return pc.GetPlayerState().GetPlayerId();
}

int ZombiesGameMode::playerSlotFromPeer(int peerSlot) const {
    if (engine_ != nullptr && engine_->GetGameInstance().IsDedicatedServer()) {
        return peerSlot;
    }
    return peerSlot + 1;
}

void ZombiesGameMode::refreshLevelIdentity(const leon::Engine& engine,
                                           const std::string& levelPath) {
    levelPath_ = levelPath;
    if (!engine.GetLevel().Name().empty()) {
        levelKey_ = engine.GetLevel().Name();
    } else if (!levelPath.empty()) {
        levelKey_ = std::filesystem::path(levelPath).stem().string();
    } else {
        levelKey_.clear();
    }
    MatchGameState().SetMapName(levelKey_);
}

std::string ZombiesGameMode::GetMapName() const {
    return levelKey_;
}

void ZombiesGameMode::NotifyClientsServerTravel(leon::Engine& engine) {
    MatchGameState().SetExpectedMapName(GetMapName());
    leon::net::SendTravelToPeers(engine.GetGameInstance().GetNetDriver(), GetMapName(),
                                 engine.GetGameInstance().IsDedicatedServer());
}

bool ZombiesGameMode::ClientTravelToHostMap(leon::Engine& engine, std::string_view hostMapName) {
    // Flow: travel
    // 1. Compare local map key to host Welcome/Travel key
    // 2. ClientTravel soft-reload if different
    // 3. Refresh identity + register bodies for the new level
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
        std::cerr << "Zombies: ClientTravel to '" << hostMapName << "' failed\n";
        return false;
    }

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
    MatchGameState().SetMapName(levelKey_);
    MatchGameState().SetExpectedMapName(std::string(hostMapName));

    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    RegisterBodiesFromLevel(engine.GetLevel());
    engine.AddOnScreenDebugMessage("ClientTravel -> '" + levelKey_ + "'", 4.0f,
                                   {0.45f, 0.95f, 1.0f});
    std::cout << "Zombies: ClientTravel to '" << levelKey_ << "'\n";
    return true;
}

void ZombiesGameMode::OnTravelFinished(leon::Engine& engine) {
    bodyTargets_.clear();
    for (PawnTarget& target : pawnTargets_) {
        target = {};
    }
    MatchGameState().SetMapName(GetMapName());

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

void ZombiesGameMode::LoginPlayer(int playerSlot, bool isLocalController) {
    if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
        return;
    }
    ZombiesPlayerController& pc = players_[static_cast<std::size_t>(playerSlot)];
    if (GetGameState().HasPlayerState(&pc.GetPlayerState()) || characterAt(playerSlot) != nullptr) {
        Logout(pc);
    }
    pc.SetPlayerState<ZombiesPlayerState>();
    pc.SetIsLocalController(isLocalController);
    pc.GetPlayerState().SetPlayerId(playerSlot);
    // Flow: PostLogin -> PlayerArray -> HandleStartingNewPlayer -> RestartPlayer.
    PostLogin(pc);
    HandleStartingNewPlayer(pc);
}

void ZombiesGameMode::logoutAllPlayers() {
    for (ZombiesPlayerController& pc : players_) {
        Logout(pc);
    }
    characters_.fill(nullptr);
}

void ZombiesGameMode::CollectZombieSpawnPoints(const leon::Level& level) {
    zombieSpawnPoints_.clear();

    // Prefer typed AISpawnPoint actors; fall back to ZombieSpawn-tagged meshes.
    for (const leon::AISpawnPoint& spawn : level.AISpawnPoints()) {
        zombieSpawnPoints_.push_back(spawn.transform.position);
    }
    if (zombieSpawnPoints_.empty()) {
        for (const leon::StaticMeshComponent& mesh : level.StaticMeshes()) {
            if (mesh.tag == kZombieSpawnTag) {
                zombieSpawnPoints_.push_back(mesh.transform.position);
            }
        }
    }
    if (!zombieSpawnPoints_.empty()) {
        return;
    }

    // Fallback ring around PlayerStart 0 / origin.
    glm::vec3 center{0.0f, matchFloorY_, 0.0f};
    const int startIdx = findPlayerStartIndex(level, 0);
    if (startIdx >= 0) {
        center = level.PlayerStarts()[static_cast<std::size_t>(startIdx)].transform.position;
        center.y = matchFloorY_;
    }
    constexpr float kRadius = 14.0f;
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        const float a =
            (static_cast<float>(i) / static_cast<float>(leon::net::kMaxAiPawns)) * 6.2831853f;
        zombieSpawnPoints_.push_back(center +
                                     glm::vec3{std::cos(a) * kRadius, 0.0f, std::sin(a) * kRadius});
    }
    std::cout << "Zombies: no AISpawnPoint / ZombieSpawn -- using fallback ring ("
              << zombieSpawnPoints_.size() << " points)\n";
}

void ZombiesGameMode::prepareMatchPhysics(leon::Engine& engine) {
    // Flow: prepare — shared match world then pack-specific spawn/interact/AI state
    PrepareMatchWorld(engine, matchFloorY_, matchWalkBounds_);
    CollectZombieSpawnPoints(engine.GetLevel());
    CollectTownInteractables(engine.GetLevel());
    ClearAIPawns(false);
    intermissionRemaining_ = 0.0f;
    bMatchGameOver_ = false;
    lavaTickAccum_ = 0.0f;
}

void ZombiesGameMode::CollectTownInteractables(const leon::Level& level) {
    CollectZombiesTownActors(level, townBuys_, townPain_);
}

void ZombiesGameMode::RebuildNavAfterDoor(leon::Engine& engine) {
    RebuildNavigation(engine, matchFloorY_, matchWalkBounds_);
}

void ZombiesGameMode::ApplyPerkToCharacter(ZombiesPlayerState& ps, ZombiesCharacter& ch,
                                           EZombiesPerk perk) {
    ps.GrantPerk(perk);
    if (perk == EZombiesPerk::Juggernog) {
        ch.SetMaxHealth(250.0f);
        ch.SetHealth(250.0f);
    } else if (perk == EZombiesPerk::SpeedCola) {
        ch.SetReloadSpeedScale(2.0f);
    } else if (perk == EZombiesPerk::DoubleTap) {
        ch.SetFireRateScale(1.55f);
    }
}

int ZombiesGameMode::FindNearestInteractable(const glm::vec3& feet, float maxDist) const {
    int best = -1;
    float bestD = maxDist;
    for (int i = 0; i < static_cast<int>(townBuys_.size()); ++i) {
        const ZombiesInteractable& buy = townBuys_[static_cast<std::size_t>(i)];
        if (buy.spent && buy.type == EZombiesInteract::Door) {
            continue;
        }
        if (buy.spent && buy.type == EZombiesInteract::Perk) {
            continue;
        }
        const float d = glm::length(glm::vec3{feet.x - buy.position.x, 0.0f, feet.z - buy.position.z});
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

std::string ZombiesGameMode::FormatInteractPrompt(const ZombiesInteractable& buy) const {
    switch (buy.type) {
    case EZombiesInteract::Door:
        return "[F] Open Door [" + std::to_string(buy.cost) + "]";
    case EZombiesInteract::WallBuy:
        return "[F] Buy " + buy.payload + " [" + std::to_string(buy.cost) + "]";
    case EZombiesInteract::Ammo:
        return "[F] Buy Ammo [" + std::to_string(buy.cost) + "]";
    case EZombiesInteract::Perk:
        return std::string("[F] ") + PerkDisplayName(ParsePerkPayload(buy.payload)) + " [" +
               std::to_string(buy.cost) + "]";
    case EZombiesInteract::PackAPunch:
        return "[F] Pack-a-Punch [" + std::to_string(buy.cost) + "]";
    }
    return {};
}

bool ZombiesGameMode::TryPurchaseInteractable(leon::Engine& engine, int playerSlot, int buyIndex) {
    if (buyIndex < 0 || buyIndex >= static_cast<int>(townBuys_.size())) {
        return false;
    }
    ZombiesInteractable& buy = townBuys_[static_cast<std::size_t>(buyIndex)];
    ZombiesCharacter* ch = characterAt(playerSlot);
    ZombiesPlayerState* ps =
        AsZombiesPS(&players_[static_cast<std::size_t>(playerSlot)].GetPlayerState());
    if (ch == nullptr || !ch->IsAlive() || ps == nullptr) {
        return false;
    }

    if (buy.type == EZombiesInteract::Door) {
        if (buy.spent || !ps->SpendScore(buy.cost)) {
            return false;
        }
        buy.spent = true;
        auto& meshes = engine.GetLevel().StaticMeshes();
        if (buy.meshIndex != leon::Level::npos && buy.meshIndex < meshes.size()) {
            meshes[buy.meshIndex].hidden = true;
            meshes[buy.meshIndex].collisionEnabled = false;
            meshes[buy.meshIndex].mesh.reset();
        }
        RebuildNavAfterDoor(engine);
        engine.AddOnScreenDebugMessage("Door opened (-" + std::to_string(buy.cost) + ")", 2.0f,
                                       {0.55f, 1.0f, 0.55f});
        return true;
    }

    if (buy.type == EZombiesInteract::WallBuy) {
        const ZombiesWeaponDef& def = FindZombiesWeapon(buy.payload);
        const bool alreadyOwns = ch->GetWeaponId() == def.id;
        const int cost = alreadyOwns ? (std::max)(100, buy.cost / 2) : buy.cost;
        if (!ps->SpendScore(cost)) {
            return false;
        }
        if (alreadyOwns) {
            ch->RefillAmmoFull();
            engine.AddOnScreenDebugMessage("Ammo refilled (-" + std::to_string(cost) + ")", 2.0f,
                                           {0.7f, 0.9f, 1.0f});
        } else {
            ch->ApplyWeapon(def, false);
            engine.AddOnScreenDebugMessage(std::string("Bought ") + def.displayName + " (-" +
                                               std::to_string(cost) + ")",
                                           2.0f, {0.7f, 0.9f, 1.0f});
        }
        return true;
    }

    if (buy.type == EZombiesInteract::Ammo) {
        if (!ps->SpendScore(buy.cost)) {
            return false;
        }
        ch->RefillAmmoFull();
        engine.AddOnScreenDebugMessage("Ammo purchased (-" + std::to_string(buy.cost) + ")", 2.0f,
                                       {0.7f, 0.9f, 1.0f});
        return true;
    }

    if (buy.type == EZombiesInteract::Perk) {
        const EZombiesPerk perk = ParsePerkPayload(buy.payload);
        if (perk == EZombiesPerk::None || ps->HasPerk(perk) || buy.spent) {
            return false;
        }
        if (!ps->SpendScore(buy.cost)) {
            return false;
        }
        ApplyPerkToCharacter(*ps, *ch, perk);
        buy.spent = true;
        engine.AddOnScreenDebugMessage(std::string("Perk: ") + PerkDisplayName(perk) + " (-" +
                                           std::to_string(buy.cost) + ")",
                                       2.5f, {0.95f, 0.75f, 0.35f});
        return true;
    }

    if (buy.type == EZombiesInteract::PackAPunch) {
        if (ch->IsWeaponPacked() || !ps->SpendScore(buy.cost)) {
            return false;
        }
        ch->PackAPunchCurrentWeapon();
        engine.AddOnScreenDebugMessage("Pack-a-Punch! (-" + std::to_string(buy.cost) + ")", 2.5f,
                                       {1.0f, 0.45f, 0.85f});
        return true;
    }
    return false;
}

void ZombiesGameMode::TickLavaDamage(leon::Engine& engine, float deltaTime) {
    if (engine.GetGameInstance().IsClient() || townPain_.empty() || bMatchGameOver_) {
        return;
    }

    std::array<bool, leon::net::kMaxPlayers> wasAlive{};
    std::array<leon::Character*, leon::net::kMaxPlayers> targets{};
    int count = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        wasAlive[static_cast<std::size_t>(i)] = ch != nullptr && ch->IsAlive();
        if (wasAlive[static_cast<std::size_t>(i)]) {
            targets[static_cast<std::size_t>(count++)] = ch;
        }
    }
    if (count <= 0) {
        return;
    }

    leon::TickPainCausingVolumes(
        townPain_, std::span<leon::Character*>{targets.data(), static_cast<std::size_t>(count)},
        deltaTime, lavaTickAccum_);

    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (!wasAlive[static_cast<std::size_t>(i)]) {
            continue;
        }
        ZombiesCharacter* ch = characterAt(i);
        if (ch != nullptr && !ch->IsAlive()) {
            HandlePlayerDeath(engine, i);
        }
    }
}

void ZombiesGameMode::TickTownInteract(leon::Engine& engine) {
    localInteractPrompt_.clear();
    if (bMatchGameOver_ || pauseMenuOpen_) {
        return;
    }

    if (!engine.GetGameInstance().IsClient()) {
        for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
            if (!players_[static_cast<std::size_t>(i)].ConsumeUseRequested()) {
                continue;
            }
            ZombiesCharacter* ch = characterAt(i);
            if (ch == nullptr || !ch->IsAlive()) {
                continue;
            }
            const int idx = FindNearestInteractable(ch->GetActorLocation(), kInteractRange);
            if (idx >= 0) {
                (void)TryPurchaseInteractable(engine, i, idx);
            }
        }
    }

    if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
        if (const ZombiesCharacter* ch = characterAt(localSlot_)) {
            if (ch->IsAlive()) {
                const int idx = FindNearestInteractable(ch->GetActorLocation(), kInteractRange);
                if (idx >= 0) {
                    localInteractPrompt_ = FormatInteractPrompt(townBuys_[static_cast<std::size_t>(idx)]);
                }
            }
        }
    }
}

void ZombiesGameMode::configurePawnForLevel(ZombiesCharacter& character,
                                            const leon::Level& /*level*/) const {
    leon::CharacterMovement& move = character.GetCharacterMovement();
    move.FloorY = matchFloorY_;
    move.WalkBounds = matchWalkBounds_;
}

void ZombiesGameMode::snapPawnToFloor(ZombiesCharacter& character, glm::vec3& inOutFeet) const {
    SnapCharacterToFloor(character, inOutFeet, matchFloorY_);
}

void ZombiesGameMode::ClearAIPawns(bool destroyPawns) {
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        aiControllers_[static_cast<std::size_t>(i)].StopMovement();
        aiControllers_[static_cast<std::size_t>(i)].UnPossess();
        if (destroyPawns && aiPawns_[static_cast<std::size_t>(i)] != nullptr) {
            GetWorld().DestroyActor(aiPawns_[static_cast<std::size_t>(i)]);
        }
        aiPawns_[static_cast<std::size_t>(i)] = nullptr;
        aiPawnTargets_[static_cast<std::size_t>(i)] = {};
        zombieMeleeCooldown_[static_cast<std::size_t>(i)] = 0.0f;
    }
    MatchGameState().SetZombiesRemaining(0);
    MatchGameState().SetRoundActive(false);
}

int ZombiesGameMode::CountAliveZombies() const {
    int n = 0;
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        const ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(i)];
        if (z != nullptr && !z->IsPendingKillPending() && z->IsAlive()) {
            ++n;
        }
    }
    return n;
}

ZombiesCharacter* ZombiesGameMode::FindNearestLivingPlayer(const glm::vec3& from) const {
    ZombiesCharacter* best = nullptr;
    float bestDistSq = 1.0e12f;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending() || !ch->IsAlive()) {
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

void ZombiesGameMode::StartNextRound(leon::Engine& engine) {
    // Flow: round
    // 1. Clear leftover AI, bump RoundIndex
    // 2. Enter intermission (~5s); SpawnZombieWave when timer elapses
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_) {
        return;
    }
    ClearAIPawns(true);
    MatchGameState().IncrementRoundIndex();
    MatchGameState().SetRoundActive(false);
    intermissionRemaining_ = kIntermissionSeconds;
    MatchGameState().SetIntermissionSeconds(static_cast<int>(std::ceil(kIntermissionSeconds)));
    const int round = MatchGameState().GetRoundIndex();
    engine.AddOnScreenDebugMessage("Round " + std::to_string(round) + " — incoming...", 4.0f,
                                   {1.0f, 0.75f, 0.35f});
    std::cout << "Zombies: StartNextRound index=" << round
              << " intermission=" << kIntermissionSeconds << "s\n";
}

void ZombiesGameMode::SpawnZombieWave(leon::Engine& engine) {
    // Flow: round
    // 1. Authority only; count = min(kMaxAiPawns, 4 + round*2)
    // 2. Spawn capsule pawns at ZombieSpawn / ring, Possess + MoveToActor
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_) {
        return;
    }
    const int round = std::max(1, MatchGameState().GetRoundIndex());
    const int count = std::min(leon::net::kMaxAiPawns, 4 + round * 2);
    ClearAIPawns(true);

    if (zombieSpawnPoints_.empty()) {
        CollectZombieSpawnPoints(engine.GetLevel());
    }

    const float zombieHp = 50.0f + static_cast<float>(round) * 10.0f;
    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        auto* enemy = GetWorld().SpawnActor<ZombieCharacter>();
        configurePawnForLevel(*enemy, engine.GetLevel());
        enemy->SetMaxHealth(zombieHp);
        enemy->Revive(zombieHp);
        leon::CharacterMovement& move = enemy->GetCharacterMovement();
        move.MaxWalkSpeed = kZombieWalkSpeed;

        const glm::vec3& spawn = zombieSpawnPoints_[static_cast<std::size_t>(
            i % static_cast<int>(zombieSpawnPoints_.size()))];
        glm::vec3 location = spawn;
        location.y = matchFloorY_;
        snapPawnToFloor(*enemy, location);
        enemy->Reset(location, 180.0f + move.ModelYawOffsetDegrees);
        enemy->SyncTransformToLevel(engine.GetLevel());

        aiPawns_[static_cast<std::size_t>(i)] = enemy;
        leon::AIController& ai = aiControllers_[static_cast<std::size_t>(i)];
        ai.SetNavigationSystem(&GetWorld().GetNavigationSystem());
        ai.SetArriveRadius(1.1f);
        ai.Possess(enemy);
        if (ZombiesCharacter* target = FindNearestLivingPlayer(location)) {
            ai.MoveToActor(target);
        }
        ++spawned;
    }

    MatchGameState().SetZombiesRemaining(spawned);
    MatchGameState().SetRoundActive(true);
    intermissionRemaining_ = 0.0f;
    MatchGameState().SetIntermissionSeconds(0);
    engine.AddOnScreenDebugMessage("Wave: " + std::to_string(spawned) + " zombies (HP " +
                                       std::to_string(static_cast<int>(zombieHp)) + ")",
                                   4.0f, {0.45f, 1.0f, 0.4f});
    std::cout << "Zombies: SpawnZombieWave n=" << spawned << " hp=" << zombieHp << '\n';
}

void ZombiesGameMode::TickRoundDirector(leon::Engine& engine, float deltaTime) {
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_ ||
        !GetGameState().HasMatchStarted()) {
        return;
    }

    if (intermissionRemaining_ > 0.0f) {
        intermissionRemaining_ -= deltaTime;
        MatchGameState().SetIntermissionSeconds(
            static_cast<int>(std::ceil(std::max(0.0f, intermissionRemaining_))));
        if (intermissionRemaining_ <= 0.0f) {
            intermissionRemaining_ = 0.0f;
            MatchGameState().SetIntermissionSeconds(0);
            SpawnZombieWave(engine);
        }
        return;
    }

    if (!MatchGameState().IsRoundActive()) {
        return;
    }

    const int alive = CountAliveZombies();
    MatchGameState().SetZombiesRemaining(alive);
    if (alive == 0) {
        StartNextRound(engine);
    }
}

ZombiesCharacter* ZombiesGameMode::EnsureClientAIProxy(leon::Engine& engine, int aiIndex) {
    if (aiIndex < 0 || aiIndex >= leon::net::kMaxAiPawns) {
        return nullptr;
    }
    ZombiesCharacter*& slot = aiPawns_[static_cast<std::size_t>(aiIndex)];
    if (slot != nullptr && !slot->IsPendingKillPending()) {
        return slot;
    }
    auto* enemy = GetWorld().SpawnActor<ZombieCharacter>();
    configurePawnForLevel(*enemy, engine.GetLevel());
    enemy->GetCharacterMovement().MaxWalkSpeed = kZombieWalkSpeed;
    slot = enemy;
    return slot;
}

void ZombiesGameMode::TickAIControllers(float deltaTime) {
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        ZombiesCharacter* enemy = aiPawns_[static_cast<std::size_t>(i)];
        if (enemy == nullptr) {
            continue;
        }
        if (enemy->IsPendingKillPending() || !enemy->IsAlive()) {
            aiControllers_[static_cast<std::size_t>(i)].UnPossess();
            if (enemy->IsPendingKillPending()) {
                aiPawns_[static_cast<std::size_t>(i)] = nullptr;
            }
            continue;
        }
        if (pauseMenuOpen_) {
            enemy->SetAnimBlendInput(0.0f);
            continue;
        }
        leon::AIController& ai = aiControllers_[static_cast<std::size_t>(i)];
        ZombiesCharacter* target = FindNearestLivingPlayer(enemy->GetActorLocation());
        const glm::vec3 wish = chaseBehavior_.Tick(ai, target, deltaTime);
        const float speedAlpha =
            enemy->IsFalling() ? 0.0f : std::clamp(glm::length(wish), 0.0f, 1.0f);
        enemy->SetAnimBlendInput(speedAlpha);
    }
}

bool ZombiesGameMode::ApplyPointDamage(ZombiesCharacter& target, float damage,
                                       int /*instigatorSlot*/) {
    if (!target.IsAlive()) {
        return false;
    }
    target.TakeDamage(damage);
    return !target.IsAlive();
}

void ZombiesGameMode::HandlePlayerDeath(leon::Engine& engine, int playerSlot) {
    if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
        return;
    }
    ZombiesPlayerController& pc = players_[static_cast<std::size_t>(playerSlot)];
    ZombiesPlayerState* ps = AsZombiesPS(&pc.GetPlayerState());
    ZombiesCharacter* ch = characterAt(playerSlot);
    if (ps == nullptr) {
        return;
    }

    // Solo Quick Revive: one free self-revive in place (Town loop).
    if (ch != nullptr && ps->TryConsumeQuickRevive()) {
        ch->Revive(ch->GetMaxHealth() * 0.5f);
        engine.AddOnScreenDebugMessage("Quick Revive!", 3.0f, {0.45f, 1.0f, 0.55f});
        return;
    }

    const bool hasLives = ps->ConsumeLifeOnDeath();
    if (hasLives) {
        RestartPlayer(pc);
        engine.AddOnScreenDebugMessage("Player " + std::to_string(playerSlot) +
                                           " down — lives left: " + std::to_string(ps->GetLives()),
                                       3.0f, {1.0f, 0.55f, 0.35f});
    } else {
        engine.AddOnScreenDebugMessage("Player " + std::to_string(playerSlot) + " out of lives",
                                       4.0f, {1.0f, 0.35f, 0.3f});
        CheckGameOver(engine);
    }
}

void ZombiesGameMode::CheckGameOver(leon::Engine& engine) {
    bool anyoneAlive = false;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const ZombiesPlayerState* ps =
            AsZombiesPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps == nullptr) {
            continue;
        }
        if (!GetGameState().HasPlayerState(ps)) {
            continue;
        }
        if (ps->GetLives() > 0) {
            anyoneAlive = true;
            break;
        }
        ZombiesCharacter* ch = characterAt(i);
        if (ch != nullptr && ch->IsAlive()) {
            anyoneAlive = true;
            break;
        }
    }
    if (anyoneAlive) {
        return;
    }
    bMatchGameOver_ = true;
    ClearAIPawns(true);
    MatchGameState().SetRoundActive(false);
    engine.AddOnScreenDebugMessage("GAME OVER — all players eliminated", 8.0f,
                                   {1.0f, 0.25f, 0.25f});
}

void ZombiesGameMode::SpawnShotFx(const glm::vec3& start, const glm::vec3& end, bool hitSomething) {
    ShotFx fx{};
    fx.start = start;
    fx.end = end;
    fx.life = kShotFxLifetime;
    fx.hit = hitSomething;
    shotFx_.push_back(fx);
}

void ZombiesGameMode::TickShotFx(leon::Engine& engine, float deltaTime) {
    if (engine.IsHeadless() || shotFx_.empty()) {
        for (ShotFx& fx : shotFx_) {
            fx.life -= deltaTime;
        }
        shotFx_.erase(std::remove_if(shotFx_.begin(), shotFx_.end(),
                                     [](const ShotFx& fx) { return fx.life <= 0.0f; }),
                      shotFx_.end());
        return;
    }

    leon::DebugDraw& draw = engine.GetRenderer().GetDebugOverlay();
    for (ShotFx& fx : shotFx_) {
        fx.life -= deltaTime;
        if (fx.life <= 0.0f) {
            continue;
        }
        const float alpha = std::clamp(fx.life / kShotFxLifetime, 0.0f, 1.0f);
        const glm::vec3 tracer = glm::vec3{1.0f, 0.75f, 0.2f} * alpha;
        draw.AddLine(fx.start, fx.end, tracer);

        // Muzzle "particles" — short radial sparks.
        const glm::vec3 forward = glm::normalize(fx.end - fx.start);
        glm::vec3 side = glm::cross(forward, glm::vec3{0.0f, 1.0f, 0.0f});
        if (glm::dot(side, side) < 1.0e-6f) {
            side = glm::vec3{1.0f, 0.0f, 0.0f};
        } else {
            side = glm::normalize(side);
        }
        const glm::vec3 up = glm::normalize(glm::cross(side, forward));
        constexpr float kSpark = 0.12f;
        const glm::vec3 sparkCol = glm::vec3{1.0f, 0.9f, 0.45f} * alpha;
        draw.AddLine(fx.start, fx.start + side * kSpark, sparkCol);
        draw.AddLine(fx.start, fx.start - side * kSpark, sparkCol);
        draw.AddLine(fx.start, fx.start + up * kSpark, sparkCol);
        draw.AddLine(fx.start, fx.start - up * kSpark, sparkCol);
        draw.AddLine(fx.start, fx.start + forward * (kSpark * 1.5f), sparkCol);

        if (fx.hit) {
            const glm::vec3 impact = glm::vec3{1.0f, 0.35f, 0.15f} * alpha;
            constexpr float kBurst = 0.2f;
            draw.AddLine(fx.end, fx.end + side * kBurst, impact);
            draw.AddLine(fx.end, fx.end - side * kBurst, impact);
            draw.AddLine(fx.end, fx.end + up * kBurst, impact);
            draw.AddLine(fx.end, fx.end - up * kBurst, impact);
            draw.AddLine(fx.end, fx.end - forward * kBurst, impact);
        }
    }
    shotFx_.erase(std::remove_if(shotFx_.begin(), shotFx_.end(),
                                 [](const ShotFx& fx) { return fx.life <= 0.0f; }),
                  shotFx_.end());
}

leon::DebugDraw* ZombiesGameMode::WeaponTraceDebugDraw(leon::Engine& engine) const {
    if (engine.IsHeadless() || !engine.IsCollisionDebugEnabled()) {
        return nullptr;
    }
    return &engine.GetRenderer().GetDebugOverlay();
}

bool ZombiesGameMode::ProcessFireForSlot(leon::Engine& engine, int playerSlot) {
    // Flow: fire
    // 1. Consume magazine round
    // 2. Eye from SpringArm GetTargetLocation (FPS socket)
    // 3. Aim = -GetBoomDirection(yaw, pitch)
    // 4. LineTrace world (F2 draws via DrawDebugType), then capsule test vs zombies
    // 5. SpawnShotFx; ApplyPointDamage; award score/kills on kill
    ZombiesCharacter* shooter = characterAt(playerSlot);
    if (shooter == nullptr || !shooter->IsAlive()) {
        return false;
    }
    if (!shooter->TryConsumeShot()) {
        return false;
    }

    const leon::SpringArmComponent& boom = shooter->SpringArm();
    const glm::vec3 eye = boom.GetTargetLocation(shooter->GetActorLocation());
    const glm::vec3 aim =
        -leon::SpringArmComponent::GetBoomDirection(boom.BoomYawDegrees, boom.BoomPitchDegrees);
    const glm::vec3 end = eye + aim * kHitscanRange;

    leon::DebugDraw* traceDraw = WeaponTraceDebugDraw(engine);
    leon::HitResult worldHit{};
    leon::CollisionQueryParams params{};
    params.bTraceFloorPlane = true;
    params.FloorY = matchFloorY_;
    if (traceDraw != nullptr) {
        params.DrawDebugType = leon::EDrawDebugTrace::ForOneFrame;
    }
    float blockT = 1.0f;
    if (leon::LineTraceSingleByChannel(GetWorld(), worldHit, eye, end,
                                       leon::ECollisionChannel::Visibility, params, traceDraw)) {
        if (worldHit.bBlockingHit) {
            blockT = std::clamp(worldHit.Time, 0.0f, 1.0f);
        }
    }

    int bestAi = -1;
    float bestT = blockT;
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(i)];
        if (z == nullptr || !z->IsAlive() || z->IsPendingKillPending()) {
            continue;
        }
        float hitT = 1.0f;
        if (!SegmentHitsCapsule(eye, end, z->GetActorLocation(), z->GetCapsule(), hitT)) {
            continue;
        }
        if (hitT < bestT) {
            bestT = hitT;
            bestAi = i;
        }
    }

    const glm::vec3 hitPos = eye + (end - eye) * bestT;
    const bool hitSomething = bestAi >= 0 || (worldHit.bBlockingHit && blockT < 0.999f);
    SpawnShotFx(eye, hitPos, hitSomething);

    if (traceDraw != nullptr && bestAi >= 0) {
        leon::HitResult pawnHit{};
        pawnHit.bBlockingHit = true;
        pawnHit.Time = bestT;
        pawnHit.Distance = bestT * kHitscanRange;
        pawnHit.ImpactPoint = hitPos;
        pawnHit.Location = hitPos;
        pawnHit.TraceStart = eye;
        pawnHit.TraceEnd = end;
        leon::DrawDebugLineTrace(*traceDraw, eye, end, {pawnHit});
    }

    if (bestAi < 0) {
        return true;
    }

    ZombiesCharacter* victim = aiPawns_[static_cast<std::size_t>(bestAi)];
    float damage = shooter->GetWeaponDamage();
    if (const ZombiesPlayerState* psShoot =
            AsZombiesPS(&players_[static_cast<std::size_t>(playerSlot)].GetPlayerState())) {
        if (psShoot->HasPerk(EZombiesPerk::DoubleTap)) {
            damage *= 1.35f;
        }
    }
    const glm::vec3 hitDir = bestT > 0.0f ? glm::normalize(end - eye) : aim;
    (void)leon::ApplyPointDamage(victim, damage, hitDir, shooter);
    if (victim->IsAlive()) {
        return true;
    }

    aiControllers_[static_cast<std::size_t>(bestAi)].UnPossess();
    GetWorld().DestroyActor(victim);
    aiPawns_[static_cast<std::size_t>(bestAi)] = nullptr;
    MatchGameState().DecrementZombiesRemaining();

    ZombiesPlayerState* ps =
        AsZombiesPS(&players_[static_cast<std::size_t>(playerSlot)].GetPlayerState());
    if (ps != nullptr) {
        ps->AddKill();
        ps->AddScore(kKillScore);
    }
    return true;
}

void ZombiesGameMode::ProcessPlayerFires(leon::Engine& engine, float deltaTime) {
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_) {
        return;
    }
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        float& cd = playerFireCooldown_[static_cast<std::size_t>(i)];
        cd = std::max(0.0f, cd - deltaTime);
        // Local pause must not keep auto-firing from a sticky LMB hold.
        if (pauseMenuOpen_ && i == localSlot_) {
            continue;
        }
        ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr || !ch->IsAlive()) {
            continue;
        }
        if (!players_[static_cast<std::size_t>(i)].IsFireHeld() || cd > 0.0f) {
            continue;
        }
        if (ch->GetAmmoInMag() <= 0 || ch->IsReloading()) {
            (void)ch->BeginReload();
            continue;
        }
        if (ProcessFireForSlot(engine, i)) {
            cd = ch->GetEffectiveFireInterval();
        }
    }
}

void ZombiesGameMode::ProcessPlayerReloads(leon::Engine& engine) {
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_) {
        return;
    }
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr || !ch->IsAlive()) {
            (void)players_[static_cast<std::size_t>(i)].ConsumeReloadRequested();
            continue;
        }
        if (players_[static_cast<std::size_t>(i)].ConsumeReloadRequested()) {
            (void)ch->BeginReload();
        }
    }
}

void ZombiesGameMode::TickPlayerWeapons(float deltaTime) {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (ZombiesCharacter* ch = characterAt(i)) {
            ch->TickWeapon(deltaTime);
        }
    }
}

void ZombiesGameMode::PredictLocalShotFx(leon::Engine& engine, ZombiesCharacter& localCh,
                                         float deltaTime) {
    float& fxCd = playerFireCooldown_[static_cast<std::size_t>(localSlot_)];
    fxCd = std::max(0.0f, fxCd - deltaTime);
    if (!localCh.IsAlive() || !players_[static_cast<std::size_t>(localSlot_)].IsFireHeld() ||
        fxCd > 0.0f) {
        return;
    }
    if (localCh.GetAmmoInMag() <= 0 || localCh.IsReloading()) {
        (void)localCh.BeginReload();
        return;
    }
    if (!localCh.TryConsumeShot()) {
        return;
    }

    const leon::SpringArmComponent& boom = localCh.SpringArm();
    const glm::vec3 eye = boom.GetTargetLocation(localCh.GetActorLocation());
    const glm::vec3 aim =
        -leon::SpringArmComponent::GetBoomDirection(boom.BoomYawDegrees, boom.BoomPitchDegrees);
    const glm::vec3 end = eye + aim * kHitscanRange;
    leon::DebugDraw* traceDraw = WeaponTraceDebugDraw(engine);
    leon::HitResult worldHit{};
    leon::CollisionQueryParams params{};
    params.bTraceFloorPlane = true;
    params.FloorY = matchFloorY_;
    if (traceDraw != nullptr) {
        params.DrawDebugType = leon::EDrawDebugTrace::ForOneFrame;
    }
    float t = 1.0f;
    if (leon::LineTraceSingleByChannel(GetWorld(), worldHit, eye, end,
                                       leon::ECollisionChannel::Visibility, params, traceDraw) &&
        worldHit.bBlockingHit) {
        t = std::clamp(worldHit.Time, 0.0f, 1.0f);
    }
    SpawnShotFx(eye, eye + (end - eye) * t, worldHit.bBlockingHit);
    fxCd = localCh.GetEffectiveFireInterval();
}

void ZombiesGameMode::TickContactMelee(leon::Engine& engine, float deltaTime) {
    if (engine.GetGameInstance().IsClient() || bMatchGameOver_) {
        return;
    }
    for (int zi = 0; zi < leon::net::kMaxAiPawns; ++zi) {
        float& cd = zombieMeleeCooldown_[static_cast<std::size_t>(zi)];
        cd = std::max(0.0f, cd - deltaTime);
        ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(zi)];
        if (z == nullptr || !z->IsAlive() || z->IsPendingKillPending() || cd > 0.0f) {
            continue;
        }
        const glm::vec3& zFeet = z->GetActorLocation();
        for (int pi = 0; pi < leon::net::kMaxPlayers; ++pi) {
            ZombiesCharacter* p = characterAt(pi);
            if (p == nullptr || !p->IsAlive() || p->IsPendingKillPending()) {
                continue;
            }
            const glm::vec3 d = p->GetActorLocation() - zFeet;
            const float reach = p->GetCapsule().radius + z->GetCapsule().radius + 0.15f;
            if ((d.x * d.x + d.z * d.z) > reach * reach) {
                continue;
            }
            if (std::abs(d.y) > kContactReach) {
                continue;
            }
            cd = kContactCooldown;
            const bool killed = ApplyPointDamage(*p, kContactDamage, -1);
            if (killed) {
                HandlePlayerDeath(engine, pi);
            }
            break;
        }
    }
}

void ZombiesGameMode::AppendPawnPrimitiveVisuals(leon::Engine& engine) const {
    if (engine.IsHeadless()) {
        return;
    }
    leon::DebugDraw& draw = engine.GetRenderer().GetDebugOverlay();

    // Per-slot accents so coop partners stay readable.
    static constexpr glm::vec3 kPlayerAccent[leon::net::kMaxPlayers] = {
        {0.3f, 0.85f, 1.0f},
        {1.0f, 0.75f, 0.25f},
        {0.55f, 1.0f, 0.45f},
        {1.0f, 0.45f, 0.85f},
    };

    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending() || !ch->IsAlive()) {
            continue;
        }
        const glm::vec3 accent =
            kPlayerAccent[static_cast<std::size_t>(i) % leon::net::kMaxPlayers];
        if (i == localSlot_) {
            DrawLocalFpViewmodel(draw, *ch);
            continue;
        }
        DrawPlayerPrimitive(draw, ch->GetActorLocation(), ch->GetActorYaw(), accent);
    }
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        const ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(i)];
        if (z == nullptr || z->IsPendingKillPending() || !z->IsAlive()) {
            continue;
        }
        DrawZombiePrimitive(draw, z->GetActorLocation(), z->GetActorYaw());
    }
}

void ZombiesGameMode::ensureMatchChrome(leon::Engine& engine) {
    if (engine.IsHeadless()) {
        return;
    }
    if (crosshair_ == nullptr) {
        crosshair_ = engine.GetHUD().AddWidget<ZombiesCrosshairWidget>();
    }
    if (matchHud_ == nullptr) {
        matchHud_ = engine.GetHUD().AddWidget<ZombiesMatchHudWidget>();
    }
}

void ZombiesGameMode::destroyMatchChrome(leon::Engine& engine) {
    if (crosshair_ != nullptr) {
        engine.GetHUD().RemoveWidget(crosshair_);
        crosshair_ = nullptr;
    }
    if (matchHud_ != nullptr) {
        engine.GetHUD().RemoveWidget(matchHud_);
        matchHud_ = nullptr;
    }
}

void ZombiesGameMode::syncMatchHud(leon::Engine& engine) {
    if (matchHud_ == nullptr && crosshair_ == nullptr) {
        return;
    }
    const leon::ENetMode mode = engine.GetGameInstance().GetNetMode();
    const bool tabOpen = scoreboard_ != nullptr && scoreboard_->IsVisible();
    const bool show = !engine.IsHeadless() && !pauseMenuOpen_ && !tabOpen &&
                      mode != leon::ENetMode::DedicatedServer;

    if (crosshair_ != nullptr) {
        crosshair_->SetVisibility(show && !bMatchGameOver_);
        float pulse = 1.0f;
        if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
            if (const ZombiesCharacter* ch = characterAt(localSlot_)) {
                const float maxHp = std::max(1.0f, ch->GetMaxHealth());
                pulse = 0.45f + 0.55f * std::clamp(ch->GetHealth() / maxHp, 0.0f, 1.0f);
            }
        }
        crosshair_->Pulse = pulse;
    }

    if (matchHud_ == nullptr) {
        return;
    }
    matchHud_->SetVisibility(show);
    matchHud_->RoundIndex = MatchGameState().GetRoundIndex();
    matchHud_->ZombiesRemaining = MatchGameState().GetZombiesRemaining();
    matchHud_->Intermission = MatchGameState().GetIntermissionSeconds() > 0;
    matchHud_->IntermissionSeconds = static_cast<float>(MatchGameState().GetIntermissionSeconds());
    matchHud_->GameOver = bMatchGameOver_;

    matchHud_->Score = 0;
    matchHud_->Lives = 0;
    matchHud_->Health = 0.0f;
    matchHud_->MaxHealth = 100.0f;
    matchHud_->AmmoInMag = 0;
    matchHud_->AmmoReserve = 0;
    matchHud_->Reloading = false;
    matchHud_->WeaponName = "M1911";
    matchHud_->InteractPrompt = localInteractPrompt_;

    if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
        if (const ZombiesCharacter* ch = characterAt(localSlot_)) {
            matchHud_->Health = ch->GetHealth();
            matchHud_->MaxHealth = ch->GetMaxHealth();
            matchHud_->AmmoInMag = ch->GetAmmoInMag();
            matchHud_->AmmoReserve = ch->GetAmmoReserve();
            matchHud_->Reloading = ch->IsReloading();
            matchHud_->WeaponName = ch->GetWeaponName();
        }
        if (const ZombiesPlayerState* ps =
                AsZombiesPS(&players_[static_cast<std::size_t>(localSlot_)].GetPlayerState())) {
            matchHud_->Score = static_cast<int>(ps->GetScore());
            matchHud_->Lives = ps->GetLives();
        }
    }
}

void ZombiesGameMode::syncSessionAddresses(leon::Engine& engine) {
    ZombiesGameInstance& session = Session(engine);
    if (session.GetLanAddress().empty()) {
        session.SetLanAddress(leon::net::DetectPrimaryLanIPv4());
    }
    if (session.GetJoinAddress().empty()) {
        session.SetJoinAddress("127.0.0.1");
    }
}

void ZombiesGameMode::FinishClientJoin(leon::Engine& engine, std::uint8_t localSlot,
                                       std::string_view hostMapName) {
    ZombiesGameInstance& session = Session(engine);
    session.SetLocalPlayerId(localSlot);
    localSlot_ = static_cast<int>(localSlot);
    if (!ClientTravelToHostMap(engine, hostMapName)) {
        session.SetClientWelcomed(false);
        return;
    }
    session.SetClientWelcomed(true);
    session.SetMatchMapName(GetMapName());
    spawnAllForClient(engine, localSlot);
    session.SetSkipNextSoftEnter(true);
    engine.AddOnScreenDebugMessage("Joined as slot " + std::to_string(localSlot) + " @ '" +
                                       GetMapName() + "'",
                                   4.0f, {0.4f, 1.0f, 0.6f});
}

void ZombiesGameMode::spawnRemotePlayer(leon::Engine& engine, int playerSlot) {
    LoginPlayer(playerSlot, false);
    engine.AddOnScreenDebugMessage("Player slot " + std::to_string(playerSlot) + " joined", 3.0f,
                                   {0.4f, 1.0f, 0.5f});
}

void ZombiesGameMode::setupNetCallbacks(leon::Engine& engine) {
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
        (void)ClientTravel(engine, ZombiesGameInstance::kMainMenuMap, levelPath_);
    });
}

void ZombiesGameMode::OnEnter(leon::Engine& engine, const std::string& levelPath) {
    engine_ = &engine;
    refreshLevelIdentity(engine, levelPath);
    engine.GetGameInstance().SetLevelBrowserVisible(false);

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
    bMatchGameOver_ = false;
    intermissionRemaining_ = 0.0f;

    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    engine.GetAudioDevice().StopMusic();

    ZombiesGameInstance& session = Session(engine);
    syncSessionAddresses(engine);
    localSlot_ = static_cast<int>(session.GetLocalPlayerId());
    if (!GetMapName().empty()) {
        session.SetMatchMapName(GetMapName());
    }

    RegisterBodiesFromLevel(engine.GetLevel());
    setupNetCallbacks(engine);

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
        } else {
            if (engine.GetGameInstance().GetNetDriver().IsConnected()) {
                leon::net::HelloMsg hello{};
                engine.GetGameInstance().GetNetDriver().SendToPeer(&hello, sizeof(hello), true);
            }
            engine.AddOnScreenDebugMessage("Waiting for host Welcome / level travel...", 4.0f,
                                           {0.55f, 0.85f, 1.0f});
        }
    } else {
        beginStandaloneMatch(engine);
    }

    engine.GetGameInstance().NotifyLevelOpened();
    ensureMatchChrome(engine);
    engine.AddOnScreenDebugMessage(
        "Match [" + GetMapName() +
            "] — hold LMB | R reload | F2 traces | Esc pause | Tab scoreboard",
        7.0f, {0.55f, 0.9f, 1.0f});
    std::cout << "Zombies: level='" << GetMapName()
              << "' mode=" << static_cast<int>(engine.GetGameInstance().GetNetMode())
              << " slot=" << localSlot_ << '\n';
}

void ZombiesGameMode::OnExit(leon::Engine& engine) {
    EndMatch();
    logoutAllPlayers();
    ClearAIPawns(false);

    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket(nullptr);
    net.SetOnPeerConnected(nullptr);
    net.SetOnPeerDisconnected(nullptr);

    if (pauseMenu_ != nullptr) {
        engine.GetHUD().RemoveWidget(pauseMenu_);
        pauseMenu_ = nullptr;
    }
    if (scoreboard_ != nullptr) {
        engine.GetHUD().RemoveWidget(scoreboard_);
        scoreboard_ = nullptr;
    }
    destroyMatchChrome(engine);
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

void ZombiesGameMode::beginStandaloneMatch(leon::Engine& engine) {
    ClearAIPawns(false);
    prepareMatchPhysics(engine);
    localSlot_ = 0;
    LoginPlayer(0, true);
    StartMatch();
    StartNextRound(engine);

    if (ZombiesCharacter* ch = characterAt(0)) {
        ch->SpringArm().SnapLagState(ch->GetActorLocation());
        ch->SpringArm().ApplyToCamera(engine.GetCamera(), ch->GetActorLocation(), 0.0f);
        ch->SetHideMeshForLocal(true);
    }
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void ZombiesGameMode::beginDedicatedMatch(leon::Engine& engine) {
    ClearAIPawns(false);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
    }
    StartMatch();
    StartNextRound(engine);
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

void ZombiesGameMode::PostLogin(leon::PlayerController& newPlayer) {
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

void ZombiesGameMode::Logout(leon::PlayerController& exiting) {
    const int slot = slotOf(exiting);
    if (slot >= 0 && slot < leon::net::kMaxPlayers) {
        players_[static_cast<std::size_t>(slot)].ClearCombatInput();
        playerFireCooldown_[static_cast<std::size_t>(slot)] = 0.0f;
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
    }
    exiting.UnPossess();
    if (slot >= 0 && slot < leon::net::kMaxPlayers) {
        if (ZombiesCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
            GetWorld().DestroyActor(existing);
            characters_[static_cast<std::size_t>(slot)] = nullptr;
        }
    }
    leon::GameMode::Logout(exiting);
}

void ZombiesGameMode::RestartPlayer(leon::PlayerController& newPlayer) {
    if (engine_ == nullptr) {
        return;
    }
    const int slot = slotOf(newPlayer);
    if (slot < 0 || slot >= leon::net::kMaxPlayers) {
        return;
    }

    newPlayer.UnPossess();
    if (ZombiesCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
        GetWorld().DestroyActor(existing);
        characters_[static_cast<std::size_t>(slot)] = nullptr;
    }

    // Capsule collision pawn — visual is DebugDraw primitives, never LoadFromCooked.
    auto* character = GetWorld().SpawnActor<ZombiesCharacter>();
    configurePawnForLevel(*character, engine_->GetLevel());
    character->ApplyWeapon(FindZombiesWeapon("M1911"), false);
    character->SetMaxHealth(100.0f);
    character->Revive(100.0f);

    if (ZombiesPlayerState* ps = AsZombiesPS(&newPlayer.GetPlayerState())) {
        if (ps->GetScore() < 1.0f) {
            ps->SetScore(kStartingPoints);
            ps->ClearPerks();
        }
        character->SetReloadSpeedScale(ps->HasPerk(EZombiesPerk::SpeedCola) ? 2.0f : 1.0f);
        character->SetFireRateScale(ps->HasPerk(EZombiesPerk::DoubleTap) ? 1.55f : 1.0f);
        if (ps->HasPerk(EZombiesPerk::Juggernog)) {
            character->SetMaxHealth(250.0f);
            character->Revive(250.0f);
        }
    }

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
        std::cerr << "Zombies: no PlayerStart for slot " << slot << " -- using fallback at floor\n";
    }

    leon::SpringArmComponent& boom = character->SpringArm();
    boom.BoomYawDegrees += yaw;
    boom.ClampPitch();
    snapPawnToFloor(*character, location);
    character->Reset(location, yaw + character->GetCharacterMovement().ModelYawOffsetDegrees);
    character->SyncTransformToLevel(engine_->GetLevel());
    characters_[static_cast<std::size_t>(slot)] = character;
    newPlayer.Possess(character);

    if (newPlayer.IsLocalController()) {
        character->SetHideMeshForLocal(true);
    }

    std::cout << "Zombies: spawn slot " << slot << " at (" << location.x << ", " << location.y
              << ", " << location.z << ") floorY=" << matchFloorY_ << '\n';
}

void ZombiesGameMode::spawnAllForListenServer(leon::Engine& engine) {
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
    if (ZombiesCharacter* host = characterAt(0)) {
        host->SpringArm().SnapLagState(host->GetActorLocation());
        host->SpringArm().ApplyToCamera(engine.GetCamera(), host->GetActorLocation(), 0.0f);
        host->SetHideMeshForLocal(true);
    }

    StartMatch();
    StartNextRound(engine);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void ZombiesGameMode::spawnAllForClient(leon::Engine& engine, std::uint8_t localSlot) {
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

    if (ZombiesCharacter* local = characterAt(localSlot_)) {
        local->SpringArm().SnapLagState(local->GetActorLocation());
        local->SpringArm().ApplyToCamera(engine.GetCamera(), local->GetActorLocation(), 0.0f);
        local->SetHideMeshForLocal(true);
    }
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);
}

void ZombiesGameMode::tryHost(leon::Engine& engine) {
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

void ZombiesGameMode::tryDedicated(leon::Engine& engine) {
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
    std::cout << "Zombies: dedicated server ready on port " << port << " -- clients Join " << tip
              << ':' << port << '\n';
}

void ZombiesGameMode::tryJoin(leon::Engine& engine, const std::string& address) {
    ZombiesGameInstance& session = Session(engine);
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

void ZombiesGameMode::handlePacket(leon::Engine& engine, int peerSlot, const std::uint8_t* data,
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
            std::cerr << "Zombies: Hello rejected (bad magic)\n";
            return;
        }
        leon::net::WelcomeMsg welcome{};
        welcome.slot = static_cast<std::uint8_t>(playerSlotFromPeer(peerSlot));
        leon::net::WriteLevelKey(welcome.levelKey, GetMapName());
        gi.GetNetDriver().SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
        std::cout << "Zombies: Welcome slot=" << static_cast<int>(welcome.slot) << " level='"
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
            std::cerr << "Zombies: Welcome rejected (bad slot)\n";
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

void ZombiesGameMode::sendSnapshot(leon::Engine& engine) {
    std::array<glm::vec3, leon::net::kMaxPlayers> viewers{};
    int viewerCount = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr || !ch->IsAlive()) {
            continue;
        }
        viewers[static_cast<std::size_t>(viewerCount++)] = ch->GetActorLocation();
    }

    leon::net::PawnSnap pawns[leon::net::kMaxSnapshotPawns]{};
    std::uint8_t pawnCount = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        if (ch == nullptr) {
            continue;
        }
        FillPawnSnap(pawns[pawnCount++], static_cast<std::uint8_t>(i), *ch);
    }
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        ZombiesCharacter* ch = aiPawns_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !ch->IsAlive()) {
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

    leon::net::SnapshotMatchMeta meta{};
    meta.roundIndex =
        static_cast<std::uint8_t>(std::clamp(MatchGameState().GetRoundIndex(), 0, 255));
    meta.unitsAlive =
        static_cast<std::uint8_t>(std::clamp(MatchGameState().GetZombiesRemaining(), 0, 255));
    meta.remainingSeconds =
        static_cast<std::uint8_t>(std::clamp(MatchGameState().GetIntermissionSeconds(), 0, 255));
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const ZombiesPlayerState* ps =
            AsZombiesPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps == nullptr || !GetGameState().HasPlayerState(ps)) {
            continue;
        }
        meta.playerScore[i] =
            static_cast<std::uint16_t>(std::clamp(static_cast<int>(ps->GetScore()), 0, 65535));
        meta.playerLives[i] = static_cast<std::uint8_t>(std::clamp(ps->GetLives(), 0, 255));
        meta.playerElims[i] = static_cast<std::uint8_t>(std::clamp(ps->GetKills(), 0, 255));
    }

    std::vector<std::uint8_t> packet;
    if (!leon::net::EncodeSnapshot(packet, GetGameState().GetReplicatedWorldTimeFrames(), pawns,
                                   pawnCount, bodies, bodyCount, &meta)) {
        return;
    }
    engine.GetGameInstance().GetNetDriver().Broadcast(packet.data(), packet.size(), false);
}

void ZombiesGameMode::applySnapshot(leon::Engine& engine, const std::uint8_t* data,
                                    std::size_t size) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }
    leon::net::DecodedSnapshot decoded;
    if (!leon::net::DecodeSnapshot(data, size, decoded)) {
        return;
    }

    GetGameState().SetReplicatedWorldTimeFrames(decoded.tick);
    MatchGameState().SetRoundIndex(decoded.RoundIndex());
    MatchGameState().SetZombiesRemaining(decoded.UnitsAlive());
    MatchGameState().SetIntermissionSeconds(decoded.RemainingSeconds());

    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesPlayerState* ps =
            AsZombiesPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps == nullptr) {
            continue;
        }
        ps->SetScore(static_cast<float>(decoded.matchMeta.playerScore[i]));
        ps->SetLives(decoded.matchMeta.playerLives[i]);
        ps->SetKills(decoded.matchMeta.playerElims[i]);
    }

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
            target.health = snap.health;
            target.grounded = snap.grounded != 0;
            target.alive = (snap.flags & leon::net::kPawnSnapAlive) != 0;
            std::uint8_t clip = 0;
            std::uint16_t reserve = 0;
            leon::net::GetPawnUserAmmo(snap, clip, reserve);
            target.ammoInMag = clip;
            target.ammoReserve = reserve;
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
        target.health = snap.health;
        target.grounded = snap.grounded != 0;
        target.alive = (snap.flags & leon::net::kPawnSnapAlive) != 0;
        target.valid = true;
    }
    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        if (aiSeen[static_cast<std::size_t>(i)]) {
            continue;
        }
        aiPawnTargets_[static_cast<std::size_t>(i)].valid = false;
        if (ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(i)]) {
            GetWorld().DestroyActor(z);
            aiPawns_[static_cast<std::size_t>(i)] = nullptr;
        }
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

void ZombiesGameMode::tickHost(leon::Engine& engine, float deltaTime) {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characters_[static_cast<std::size_t>(i)];
        if (ch == nullptr) {
            continue;
        }
        if (!ch->IsAlive()) {
            ch->SetAnimBlendInput(0.0f);
            continue;
        }
        if (pauseMenuOpen_ && i == localSlot_) {
            ch->SetAnimBlendInput(0.0f);
            continue;
        }
        const glm::vec3 move = players_[static_cast<std::size_t>(i)].TickInput(engine);
        const float speedAlpha = ch->IsFalling() ? 0.0f : std::clamp(glm::length(move), 0.0f, 1.0f);
        ch->SetAnimBlendInput(speedAlpha);
    }

    ProcessPlayerReloads(engine);
    TickPlayerWeapons(deltaTime);
    ProcessPlayerFires(engine, deltaTime);
    TickTownInteract(engine);
    TickLavaDamage(engine, deltaTime);
    TickAIControllers(deltaTime);
    TickContactMelee(engine, deltaTime);
    TickRoundDirector(engine, deltaTime);

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
    AppendPawnPrimitiveVisuals(engine);
    TickShotFx(engine, deltaTime);

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

void ZombiesGameMode::tickDedicatedSpectator(leon::Engine& /*engine*/, float /*deltaTime*/) {}

void ZombiesGameMode::tickClient(leon::Engine& engine, float deltaTime) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }

    if (!pauseMenuOpen_ && characterAt(localSlot_) != nullptr) {
        players_[static_cast<std::size_t>(localSlot_)].TickInput(engine);
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].ConsumeLocalInputCmd();
        // Reliable only on jump/reload edges and fire press — not every held-fire frame.
        const bool fireHeld = leon::net::HasInputButton(cmd, leon::net::InputButtons::Fire);
        const bool fireEdge = fireHeld && !lastSentFire_;
        lastSentFire_ = fireHeld;
        const bool reliable = cmd.jump != 0 ||
                              leon::net::HasInputButton(cmd, leon::net::InputButtons::Reload) ||
                              leon::net::HasInputButton(cmd, leon::net::InputButtons::Use) ||
                              fireEdge;
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), reliable);

        ZombiesCharacter* localCh = characterAt(localSlot_);
        if (localCh != nullptr && localCh->IsAlive()) {
            if (players_[static_cast<std::size_t>(localSlot_)].ConsumeReloadRequested()) {
                (void)localCh->BeginReload();
            }
            localCh->TickWeapon(deltaTime);
            PredictLocalShotFx(engine, *localCh, deltaTime);
        }
    } else if (characterAt(localSlot_) != nullptr) {
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].MakeIdleInputCmd();
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), false);
    }

    const float a = ExpAlpha(14.0f, deltaTime);
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        ZombiesCharacter* ch = characterAt(i);
        const PawnTarget& t = pawnTargets_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !t.valid) {
            continue;
        }
        const glm::vec3 pos = glm::mix(ch->GetActorLocation(), t.pos, a);
        const float yaw = LerpAngleDegrees(ch->GetActorYaw(), t.yaw, a);
        ch->ApplyReplicatedState(pos, yaw, t.velY, t.grounded);
        ch->SetAnimBlendInput(t.animBlend);
        ch->SetHealth(t.health);
        if (!t.alive && ch->IsAlive()) {
            ch->SetHealth(0.0f);
        }
        if (i == localSlot_) {
            const bool hardSync =
                !players_[static_cast<std::size_t>(localSlot_)].IsFireHeld() &&
                playerFireCooldown_[static_cast<std::size_t>(localSlot_)] <= 0.0f &&
                !ch->IsReloading();
            ch->ReconcileAmmoFromAuthority(t.ammoInMag, t.ammoReserve, hardSync);
        } else {
            ch->SetAmmo(t.ammoInMag, t.ammoReserve);
        }
        if (i != localSlot_) {
            ch->SpringArm().BoomYawDegrees =
                LerpAngleDegrees(ch->SpringArm().BoomYawDegrees, t.boomYaw, a);
            ch->SpringArm().BoomPitchDegrees =
                glm::mix(ch->SpringArm().BoomPitchDegrees, t.boomPitch, a);
        }
        ch->Tick(deltaTime);
        ch->SyncTransformToLevel(engine.GetLevel());
    }

    for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
        ZombiesCharacter* ch = aiPawns_[static_cast<std::size_t>(i)];
        const PawnTarget& t = aiPawnTargets_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !t.valid) {
            continue;
        }
        const glm::vec3 pos = glm::mix(ch->GetActorLocation(), t.pos, a);
        const float yaw = LerpAngleDegrees(ch->GetActorYaw(), t.yaw, a);
        ch->ApplyReplicatedState(pos, yaw, t.velY, t.grounded);
        ch->SetAnimBlendInput(t.animBlend);
        ch->SetHealth(t.health);
        ch->Tick(deltaTime);
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

    AppendPawnPrimitiveVisuals(engine);
    if (!engine.IsHeadless() && engine.IsCollisionDebugEnabled()) {
        leon::DebugDraw& draw = engine.GetRenderer().GetDebugOverlay();
        for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
            if (ZombiesCharacter* ch = characterAt(i)) {
                GetWorld().GetPhysicsScene().AppendCollisionDebug(
                    draw, ch->GetCapsule(), ch->GetActorLocation(), ch->LevelMeshIndex());
            }
        }
        for (int i = 0; i < leon::net::kMaxAiPawns; ++i) {
            if (ZombiesCharacter* z = aiPawns_[static_cast<std::size_t>(i)]) {
                GetWorld().GetPhysicsScene().AppendCollisionDebug(
                    draw, z->GetCapsule(), z->GetActorLocation(), z->LevelMeshIndex());
            }
        }
    }
    TickShotFx(engine, deltaTime);
    TickTownInteract(engine);

    if (!pauseMenuOpen_) {
        players_[static_cast<std::size_t>(localSlot_)].UpdateCamera(engine, deltaTime);
    }
}

void ZombiesGameMode::updateNetHud(leon::Engine& engine) {
    syncMatchHud(engine);

    if (engine.IsHeadless() || pauseMenuOpen_) {
        return;
    }
    if (scoreboard_ != nullptr && scoreboard_->IsVisible()) {
        engine.ClearCenterHudText();
        return;
    }

    const leon::ENetMode mode = engine.GetGameInstance().GetNetMode();
    const std::string tip = Session(engine).GetJoinAddress();

    // Match chrome lives in ZombiesMatchHudWidget; center line is connection / tips only.
    if (mode == leon::ENetMode::DedicatedServer) {
        const std::uint16_t port = engine.GetGameInstance().PendingDedicatedPort();
        engine.SetCenterHudText("DEDICATED " + tip + ":" + std::to_string(port) + "  [" +
                                GetMapName() + "]  Esc=pause  Tab=players");
    } else if (mode == leon::ENetMode::ListenServer) {
        engine.ClearCenterHudText();
    } else if (mode == leon::ENetMode::Client) {
        if (Session(engine).IsClientWelcomed()) {
            engine.ClearCenterHudText();
        } else {
            engine.SetCenterHudText("CLIENT joining " + tip + ":7777...");
        }
    } else {
        engine.SetCenterHudText("Esc pause | Tab | H Listen | C Join " + tip + " | V | F9");
    }
}

std::string ZombiesGameMode::buildScoreboardText(const leon::Engine& engine) const {
    const leon::GameState& gs = GetGameState();
    const leon::ENetMode mode = engine.GetGameInstance().GetNetMode();
    const std::string& mapName = !gs.GetMapName().empty() ? gs.GetMapName() : GetMapName();

    std::string text = "SCOREBOARD";
    if (mode == leon::ENetMode::ListenServer) {
        text += "  (Listen Host)";
    } else if (mode == leon::ENetMode::DedicatedServer) {
        text += "  (Dedicated)";
    } else if (mode == leon::ENetMode::Client) {
        text += "  (Client)";
    } else {
        text += "  (Standalone)";
    }
    text += "\n[" + mapName + "]  Round " + std::to_string(MatchGameState().GetRoundIndex());
    text += "\n  Name            Score  Kills  Lives";

    const auto& playerArray = gs.GetPlayerArray();
    if (playerArray.empty()) {
        if (mode == leon::ENetMode::Client && !Session(engine).IsClientWelcomed()) {
            text += "\n  Connecting...";
        } else {
            text += "\n  (none connected)";
        }
    } else {
        for (const leon::PlayerState* base : playerArray) {
            if (base == nullptr) {
                continue;
            }
            std::string name = base->GetPlayerName();
            if (name.empty()) {
                const bool dedicated = mode == leon::ENetMode::DedicatedServer;
                name = (base->GetPlayerId() == 0 && !dedicated)
                           ? "Host"
                           : ("Player " + std::to_string(base->GetPlayerId()));
            }
            const ZombiesPlayerState* ps = AsZombiesPS(base);
            const int score = static_cast<int>(ps != nullptr ? ps->GetScore() : base->GetScore());
            const int kills = ps != nullptr ? ps->GetKills() : 0;
            const int lives = ps != nullptr ? ps->GetLives() : 0;
            text += "\n  " + name;
            if (mode != leon::ENetMode::DedicatedServer && base->GetPlayerId() == localSlot_) {
                text += " (You)";
            }
            text += "   " + std::to_string(score) + "    " + std::to_string(kills) + "     " +
                    std::to_string(lives);
        }
    }
    text += "\n(hold Tab)";
    return text;
}

void ZombiesGameMode::updateScoreboard(leon::Engine& engine, bool tabHeld) {
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

void ZombiesGameMode::openPauseMenu(leon::Engine& engine) {
    pauseMenuOpen_ = true;
    if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
        (void)players_[static_cast<std::size_t>(localSlot_)].MakeIdleInputCmd();
    }
    lastSentFire_ = false;
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

void ZombiesGameMode::closePauseMenu(leon::Engine& engine) {
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

void ZombiesGameMode::returnToMenu(leon::Engine& engine) {
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
        (void)ClientTravel(engine, ZombiesGameInstance::kMainMenuMap, levelPath_);
    } else {
        (void)ServerTravel(engine, ZombiesGameInstance::kMainMenuMap, levelPath_);
    }
}

void ZombiesGameMode::Tick(leon::Engine& engine, float deltaTime) {
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
        ZombiesCharacter* ch = characterAt(0);
        if (ch == nullptr || ch->IsPendingKillPending()) {
            return;
        }
        if (ch->IsAlive() && !pauseMenuOpen_) {
            const glm::vec3 move = players_[0].TickInput(engine);
            const float speedAlpha =
                ch->IsFalling() ? 0.0f : std::clamp(glm::length(move), 0.0f, 1.0f);
            ch->SetAnimBlendInput(speedAlpha);
        } else {
            ch->SetAnimBlendInput(0.0f);
        }

        ProcessPlayerReloads(engine);
        TickPlayerWeapons(deltaTime);
        ProcessPlayerFires(engine, deltaTime);
        TickAIControllers(deltaTime);
        TickContactMelee(engine, deltaTime);
        TickRoundDirector(engine, deltaTime);

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
        AppendPawnPrimitiveVisuals(engine);
        TickShotFx(engine, deltaTime);
        if (!pauseMenuOpen_) {
            players_[0].UpdateCamera(engine, deltaTime);
        }
    }
}

} // namespace game
