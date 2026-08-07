#include "FurytoonGameMode.h"

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
#include <leon/core/Camera.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>
#include <leon/level/Level.h>
#include <leon/net/NetDriver.h>
#include <leon/net/NetUtil.h>
#include <leon/net/RootReplication.h>
#include <leon/net/SnapshotCodec.h>
#include <leon/physics/CollisionShape.h>
#include <leon/physics/PhysScene.h>
#include <string>
#include <string_view>
#include <vector>

#include "FurytoonCharacter.h"
#include "FurytoonGameInstance.h"
#include "FurytoonPlayerState.h"

namespace game {
namespace {

[[nodiscard]] FurytoonGameInstance& Session(leon::Engine& engine) {
    return FurytoonGameInstance::Get(engine.GetGameInstance());
}
[[nodiscard]] const FurytoonGameInstance& Session(const leon::Engine& engine) {
    return FurytoonGameInstance::Get(engine.GetGameInstance());
}

constexpr float kMeleeRange = 1.5f;
constexpr float kLightKnockback = 0.55f;
constexpr float kHeavyKnockback = 1.05f;
constexpr int kDefaultStocks = 3;

float ExpAlpha(float speed, float dt) {
    return (speed <= 0.0f || dt <= 0.0f) ? 1.0f : (1.0f - std::exp(-speed * dt));
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

[[nodiscard]] const glm::vec3& FighterAccent(int slot) {
    static constexpr glm::vec3 kAccent[leon::net::kMaxPlayers] = {
        {0.3f, 0.85f, 1.0f},
        {1.0f, 0.75f, 0.25f},
        {0.55f, 1.0f, 0.4f},
        {1.0f, 0.45f, 0.85f},
    };
    const int i = std::clamp(slot, 0, leon::net::kMaxPlayers - 1);
    return kAccent[static_cast<std::size_t>(i)];
}

void EnableSharedArenaCamera(leon::Engine& engine) {
    engine.GetCamera().SetMode(leon::ECameraMode::Orbit);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
}

void FillPawnSnap(leon::net::PawnSnap& snap, std::uint8_t slot, const FurytoonCharacter& ch) {
    snap = leon::net::CaptureCharacterRoot(slot, ch, ch.SpringArm().BoomYawDegrees,
                                           ch.SpringArm().BoomPitchDegrees);
    snap.health = ch.GetHealth();
    snap.flags = ch.IsAlive() ? leon::net::kPawnSnapAlive : 0;
    leon::net::SetPawnUserAmmo(snap, 0, 0);
}

[[nodiscard]] FurytoonPlayerState* AsFurytoonPS(leon::PlayerState* ps) {
    return dynamic_cast<FurytoonPlayerState*>(ps);
}
[[nodiscard]] const FurytoonPlayerState* AsFurytoonPS(const leon::PlayerState* ps) {
    return dynamic_cast<const FurytoonPlayerState*>(ps);
}

[[nodiscard]] float DistXZ(const glm::vec3& a, const glm::vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

/// Pawn-vs-pawn melee reach. CapsuleTrace/SweepCapsuleAlongSegment only hit WorldStatic
/// (no HitActor), so character hits stay geometric to preserve combat feel.
[[nodiscard]] bool ForwardReachHitsCapsule(const glm::vec3& origin, const glm::vec3& tip,
                                           const glm::vec3& feet, float radius, float height) {
    const glm::vec3 mid = feet + glm::vec3{0.0f, height * 0.5f, 0.0f};
    const glm::vec3 ab = tip - origin;
    const float abLenSq = glm::dot(ab, ab);
    const float t =
        abLenSq > 1.0e-8f ? std::clamp(glm::dot(mid - origin, ab) / abLenSq, 0.0f, 1.0f) : 0.0f;
    const glm::vec3 d = mid - (origin + ab * t);
    const float r = radius + 0.25f;
    return glm::dot(d, d) <= r * r;
}

} // namespace

void FurytoonGameMode::InitGameState() {}

FurytoonGameState& FurytoonGameMode::MatchGameState() {
    FurytoonGameState* gs = GetGameState<FurytoonGameState>();
    return gs != nullptr ? *gs : static_cast<FurytoonGameState&>(GetGameState());
}
const FurytoonGameState& FurytoonGameMode::MatchGameState() const {
    const FurytoonGameState* gs = GetGameState<FurytoonGameState>();
    return gs != nullptr ? *gs : static_cast<const FurytoonGameState&>(GetGameState());
}

bool FurytoonGameMode::Matches(const leon::LevelEntry& /*entry*/,
                               const std::string& gameModeId) const {
    return gameModeId == Id() || gameModeId == "Furytoon" || gameModeId == "Kitchen";
}

int FurytoonGameMode::findPlayerStartIndex(const leon::Level& level, int slot) const {
    const auto& starts = level.PlayerStarts();
    return starts.empty() ? -1 : std::clamp(slot, 0, static_cast<int>(starts.size()) - 1);
}

FurytoonCharacter* FurytoonGameMode::characterAt(int slot) const {
    return (slot < 0 || slot >= leon::net::kMaxPlayers)
               ? nullptr
               : characters_[static_cast<std::size_t>(slot)];
}

int FurytoonGameMode::slotOf(const leon::PlayerController& pc) const {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (&players_[static_cast<std::size_t>(i)] == &pc) {
            return i;
        }
    }
    return pc.GetPlayerState().GetPlayerId();
}

int FurytoonGameMode::playerSlotFromPeer(int peerSlot) const {
    if (engine_ != nullptr && engine_->GetGameInstance().IsDedicatedServer()) {
        return peerSlot;
    }
    return peerSlot + 1;
}

void FurytoonGameMode::refreshLevelIdentity(const leon::Engine& engine,
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

std::string FurytoonGameMode::GetMapName() const {
    return levelKey_;
}

void FurytoonGameMode::NotifyClientsServerTravel(leon::Engine& engine) {
    MatchGameState().SetExpectedMapName(GetMapName());
    leon::net::SendTravelToPeers(engine.GetGameInstance().GetNetDriver(), GetMapName(),
                                 engine.GetGameInstance().IsDedicatedServer());
}

bool FurytoonGameMode::ClientTravelToHostMap(leon::Engine& engine, std::string_view hostMapName) {
    if (hostMapName.empty() || LevelKeysMatch(GetMapName(), hostMapName)) {
        return true;
    }
    applyingTravel_ = true;
    const bool ok = ClientTravel(engine, hostMapName, levelPath_);
    applyingTravel_ = false;
    if (!ok) {
        engine.AddOnScreenDebugMessage("ClientTravel failed -- '" + std::string(hostMapName) +
                                           "' missing",
                                       6.0f, {1.0f, 0.35f, 0.3f});
        return false;
    }
    if (!levelPath_.empty()) {
        namespace fs = std::filesystem;
        const fs::path dir = fs::path(levelPath_).parent_path();
        const std::string needle = leon::AsciiToLower(hostMapName);
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!ec && entry.is_regular_file() &&
                leon::AsciiToLower(entry.path().extension().string()) == ".llev" &&
                leon::AsciiToLower(entry.path().stem().string()) == needle) {
                levelPath_ = entry.path().lexically_normal().string();
                break;
            }
        }
    }
    refreshLevelIdentity(engine, levelPath_);
    if (GetMapName().empty()) {
        levelKey_ = std::string(hostMapName);
    }
    MatchGameState().SetMapName(levelKey_);
    MatchGameState().SetExpectedMapName(std::string(hostMapName));
    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    RegisterBodiesFromLevel(engine.GetLevel());
    return true;
}

void FurytoonGameMode::OnTravelFinished(leon::Engine& engine) {
    for (PawnTarget& target : pawnTargets_) {
        target = {};
    }
    MatchGameState().SetMapName(GetMapName());
    if (engine.GetGameInstance().IsDedicatedServer()) {
        beginDedicatedMatch(engine);
        const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
        for (int peer = 0; peer < peers && peer < leon::net::kMaxPlayers; ++peer) {
            spawnRemotePlayer(engine, playerSlotFromPeer(peer));
        }
        FillEmptySlotsWithBots(engine);
        return;
    }
    if (engine.GetGameInstance().IsListenServer()) {
        spawnAllForListenServer(engine);
        const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
        for (int peer = 0; peer < peers; ++peer) {
            spawnRemotePlayer(engine, playerSlotFromPeer(peer));
        }
        FillEmptySlotsWithBots(engine);
        return;
    }
    if (engine.GetGameInstance().IsClient() && Session(engine).IsClientWelcomed()) {
        spawnAllForClient(engine, static_cast<std::uint8_t>(localSlot_));
        return;
    }
    beginStandaloneMatch(engine);
}

void FurytoonGameMode::LoginPlayer(int playerSlot, bool isLocalController) {
    if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
        return;
    }
    slotIsBot_[static_cast<std::size_t>(playerSlot)] = false;
    botControllers_[static_cast<std::size_t>(playerSlot)].StopMovement();
    botControllers_[static_cast<std::size_t>(playerSlot)].UnPossess();

    FurytoonPlayerController& pc = players_[static_cast<std::size_t>(playerSlot)];
    if (GetGameState().HasPlayerState(&pc.GetPlayerState()) || characterAt(playerSlot) != nullptr) {
        Logout(pc);
    }
    pc.SetPlayerState<FurytoonPlayerState>();
    pc.SetIsLocalController(isLocalController);
    pc.GetPlayerState().SetPlayerId(playerSlot);
    if (FurytoonPlayerState* ps = AsFurytoonPS(&pc.GetPlayerState())) {
        ps->SetStocks(kDefaultStocks);
        ps->SetKOs(0);
    }
    PostLogin(pc);
    HandleStartingNewPlayer(pc);
}

void FurytoonGameMode::logoutAllPlayers() {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        botControllers_[static_cast<std::size_t>(i)].StopMovement();
        botControllers_[static_cast<std::size_t>(i)].UnPossess();
        slotIsBot_[static_cast<std::size_t>(i)] = false;
        Logout(players_[static_cast<std::size_t>(i)]);
    }
    characters_.fill(nullptr);
}

void FurytoonGameMode::destroyAllFighterMeshes(leon::Engine& engine) {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (FurytoonCharacter* ch = characters_[static_cast<std::size_t>(i)]) {
            ch->DestroyPrimitiveMeshes(engine.GetLevel());
        }
    }
}

void FurytoonGameMode::prepareMatchPhysics(leon::Engine& engine) {
    PrepareMatchWorld(engine, matchFloorY_, matchWalkBounds_);
    bMatchOver_ = false;
}

void FurytoonGameMode::configurePawnForLevel(FurytoonCharacter& character,
                                             const leon::Level& /*level*/) const {
    character.GetCharacterMovement().FloorY = matchFloorY_;
    character.GetCharacterMovement().WalkBounds = matchWalkBounds_;
}

void FurytoonGameMode::snapPawnToFloor(FurytoonCharacter& character, glm::vec3& inOutFeet) const {
    SnapCharacterToFloor(character, inOutFeet, matchFloorY_);
}

int FurytoonGameMode::CountLivingFighters() const {
    int n = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const FurytoonPlayerState* ps =
            AsFurytoonPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps == nullptr || !GetGameState().HasPlayerState(ps)) {
            continue;
        }
        if (ps->GetStocks() > 0) {
            ++n;
            continue;
        }
        if (const FurytoonCharacter* ch = characterAt(i); ch != nullptr && ch->IsAlive()) {
            ++n;
        }
    }
    return n;
}

void FurytoonGameMode::FillEmptySlotsWithBots(leon::Engine& engine) {
    if (engine.GetGameInstance().IsClient() || bMatchOver_) {
        return;
    }
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        if (characterAt(slot) != nullptr) {
            continue;
        }
        slotIsBot_[static_cast<std::size_t>(slot)] = true;
        FurytoonPlayerController& pc = players_[static_cast<std::size_t>(slot)];
        if (GetGameState().HasPlayerState(&pc.GetPlayerState())) {
            Logout(pc);
        }
        pc.SetPlayerState<FurytoonPlayerState>();
        pc.SetIsLocalController(false);
        pc.GetPlayerState().SetPlayerId(slot);
        pc.GetPlayerState().SetPlayerName("Bot " + std::to_string(slot));
        if (FurytoonPlayerState* ps = AsFurytoonPS(&pc.GetPlayerState())) {
            ps->SetStocks(kDefaultStocks);
            ps->SetKOs(0);
        }
        PostLogin(pc);
        RestartPlayer(pc);
    }
    MatchGameState().SetFightersAlive(CountLivingFighters());
}

void FurytoonGameMode::TickBotAI(float deltaTime) {
    if (engine_ == nullptr || engine_->GetGameInstance().IsClient() || bMatchOver_) {
        return;
    }
    static std::array<float, leon::net::kMaxPlayers> botAttackCd{};
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        if (!slotIsBot_[static_cast<std::size_t>(slot)]) {
            continue;
        }
        FurytoonCharacter* self = characterAt(slot);
        if (self == nullptr || !self->IsAlive()) {
            continue;
        }
        leon::AIController& ai = botControllers_[static_cast<std::size_t>(slot)];
        ai.SetNavigationSystem(&GetWorld().GetNavigationSystem());

        FurytoonCharacter* nearest = nullptr;
        float best = 1.0e12f;
        for (int other = 0; other < leon::net::kMaxPlayers; ++other) {
            if (other == slot) {
                continue;
            }
            FurytoonCharacter* foe = characterAt(other);
            if (foe == nullptr || !foe->IsAlive()) {
                continue;
            }
            const float d = DistXZ(self->GetActorLocation(), foe->GetActorLocation());
            if (d < best) {
                best = d;
                nearest = foe;
            }
        }
        if (nearest != nullptr) {
            (void)chaseBehavior_.Tick(ai, nearest, deltaTime);
        } else {
            (void)chaseBehavior_.Tick(ai, nullptr, deltaTime);
        }

        float& cd = botAttackCd[static_cast<std::size_t>(slot)];
        cd = std::max(0.0f, cd - deltaTime);
        if (nearest != nullptr && best <= kMeleeRange && cd <= 0.0f && !self->IsAttacking()) {
            const bool heavy = (static_cast<int>(best * 10.0f) % 3) == 0;
            if (self->TryStartAttack(heavy ? EFurytoonAttack::Heavy : EFurytoonAttack::Light)) {
                cd = heavy ? 0.85f : 0.45f;
            }
        }
    }
}

void FurytoonGameMode::ProcessAttacksForSlot(leon::Engine& /*engine*/, int slot) {
    FurytoonCharacter* ch = characterAt(slot);
    if (ch == nullptr || !ch->IsAlive()) {
        return;
    }
    if (!slotIsBot_[static_cast<std::size_t>(slot)]) {
        FurytoonPlayerController& pc = players_[static_cast<std::size_t>(slot)];
        if (pc.ConsumeLightAttack()) {
            (void)ch->TryStartAttack(EFurytoonAttack::Light);
        }
        if (pc.ConsumeHeavyAttack()) {
            (void)ch->TryStartAttack(EFurytoonAttack::Heavy);
        }
    }
    if (ch->ConsumeHitWindow()) {
        ApplyMeleeHit(*engine_, slot);
    }
}

void FurytoonGameMode::ProcessCombat(leon::Engine& engine, float deltaTime) {
    if (engine.GetGameInstance().IsClient() || bMatchOver_) {
        return;
    }
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (FurytoonCharacter* ch = characterAt(i)) {
            ch->TickCombat(deltaTime);
        }
        ProcessAttacksForSlot(engine, i);
    }
}

void FurytoonGameMode::ApplyMeleeHit(leon::Engine& engine, int attackerSlot) {
    FurytoonCharacter* attacker = characterAt(attackerSlot);
    if (attacker == nullptr || !attacker->IsAlive()) {
        return;
    }
    glm::vec3 forward{}, right{};
    YawAxes(attacker->GetActorYaw(), forward, right);
    (void)right;
    const float reach = attacker->GetAttackReach();
    const float damage = attacker->GetAttackDamage();
    const float knock =
        attacker->GetAttackKind() == EFurytoonAttack::Heavy ? kHeavyKnockback : kLightKnockback;
    const glm::vec3 origin =
        attacker->GetActorLocation() + glm::vec3{0.0f, FurytoonCharacter::kAttackHalfHeight, 0.0f};
    const glm::vec3 tip = origin + forward * reach;

    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (i == attackerSlot) {
            continue;
        }
        FurytoonCharacter* victim = characterAt(i);
        if (victim == nullptr || !victim->IsAlive()) {
            continue;
        }
        const leon::CapsuleShape& cap = victim->GetCapsule();
        if (!ForwardReachHitsCapsule(origin, tip, victim->GetActorLocation(), cap.radius,
                                     cap.height)) {
            continue;
        }
        leon::ApplyPointDamage(victim, damage, forward, attacker);
        victim->ApplyHitReaction(attacker->GetActorLocation(), knock);
        if (!victim->IsAlive()) {
            HandleFighterKO(engine, i, attackerSlot);
        }
        break;
    }
}

void FurytoonGameMode::HandleFighterKO(leon::Engine& engine, int victimSlot, int attackerSlot) {
    if (victimSlot < 0 || victimSlot >= leon::net::kMaxPlayers) {
        return;
    }
    FurytoonPlayerController& pc = players_[static_cast<std::size_t>(victimSlot)];
    FurytoonPlayerState* victimPs = AsFurytoonPS(&pc.GetPlayerState());
    if (victimPs == nullptr) {
        return;
    }
    if (attackerSlot >= 0 && attackerSlot < leon::net::kMaxPlayers) {
        if (FurytoonPlayerState* atk =
                AsFurytoonPS(&players_[static_cast<std::size_t>(attackerSlot)].GetPlayerState())) {
            atk->AddKO();
        }
    }
    if (victimPs->ConsumeStock()) {
        RestartPlayer(pc);
        engine.AddOnScreenDebugMessage("KO — stocks " + std::to_string(victimPs->GetStocks()), 2.5f,
                                       {1.0f, 0.6f, 0.35f});
    } else {
        engine.AddOnScreenDebugMessage("Eliminated slot " + std::to_string(victimSlot), 3.0f,
                                       {1.0f, 0.35f, 0.3f});
    }
    MatchGameState().SetFightersAlive(CountLivingFighters());
    CheckMatchOver(engine);
}

void FurytoonGameMode::CheckMatchOver(leon::Engine& engine) {
    if (bMatchOver_ || engine.GetGameInstance().IsClient()) {
        return;
    }
    const int alive = CountLivingFighters();
    MatchGameState().SetFightersAlive(alive);
    if (alive > 1) {
        return;
    }
    bMatchOver_ = true;
    int winner = -1;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const FurytoonPlayerState* ps =
            AsFurytoonPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps != nullptr && GetGameState().HasPlayerState(ps) && ps->GetStocks() > 0) {
            winner = i;
            break;
        }
    }
    engine.AddOnScreenDebugMessage(winner >= 0 ? ("Match over — winner " + std::to_string(winner))
                                               : "Match over — draw",
                                   8.0f, {1.0f, 0.9f, 0.35f});
}

void FurytoonGameMode::AppendFighterVisuals(leon::Engine& engine) const {
    if (engine.IsHeadless()) {
        return;
    }
    leon::Level& level = engine.GetLevel();
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        FurytoonCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending()) {
            continue;
        }
        ch->SyncPrimitiveMeshes(level);
    }
}

void FurytoonGameMode::EnsureFighterAccent(int slot, FurytoonCharacter& character) {
    character.SetAccentColor(FighterAccent(slot));
}

void FurytoonGameMode::UpdateSharedArenaCamera(leon::Engine& engine, float deltaTime) {
    if (engine.IsHeadless() || engine.GetGameInstance().IsDedicatedServer()) {
        return;
    }

    std::vector<glm::vec3> livingFeet;
    livingFeet.reserve(static_cast<std::size_t>(leon::net::kMaxPlayers));
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const FurytoonCharacter* ch = characterAt(i);
        if (ch == nullptr || ch->IsPendingKillPending() || !ch->IsAlive()) {
            continue;
        }
        livingFeet.push_back(ch->GetActorLocation());
    }

    // Match prior lag (7) while using shared ArenaCamera framing defaults otherwise.
    leon::ArenaCameraParams params{};
    params.lagSpeed = 7.0f;
    leon::UpdateArenaCamera(engine.GetCamera(), arenaCamera_, params, livingFeet, deltaTime,
                            matchFloorY_);
}

void FurytoonGameMode::syncSessionAddresses(leon::Engine& engine) {
    FurytoonGameInstance& session = Session(engine);
    if (session.GetLanAddress().empty()) {
        session.SetLanAddress(leon::net::DetectPrimaryLanIPv4());
    }
    if (session.GetJoinAddress().empty()) {
        session.SetJoinAddress("127.0.0.1");
    }
}

void FurytoonGameMode::FinishClientJoin(leon::Engine& engine, std::uint8_t localSlot,
                                        std::string_view hostMapName) {
    FurytoonGameInstance& session = Session(engine);
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
    engine.AddOnScreenDebugMessage("Joined slot " + std::to_string(localSlot), 4.0f,
                                   {0.4f, 1.0f, 0.6f});
}

void FurytoonGameMode::spawnRemotePlayer(leon::Engine& engine, int playerSlot) {
    if (playerSlot < 0 || playerSlot >= leon::net::kMaxPlayers) {
        return;
    }
    if (slotIsBot_[static_cast<std::size_t>(playerSlot)] || characterAt(playerSlot) != nullptr) {
        botControllers_[static_cast<std::size_t>(playerSlot)].StopMovement();
        botControllers_[static_cast<std::size_t>(playerSlot)].UnPossess();
        Logout(players_[static_cast<std::size_t>(playerSlot)]);
        slotIsBot_[static_cast<std::size_t>(playerSlot)] = false;
    }
    LoginPlayer(playerSlot, false);
    engine.AddOnScreenDebugMessage("Player slot " + std::to_string(playerSlot) + " joined", 2.5f,
                                   {0.4f, 1.0f, 0.5f});
}

void FurytoonGameMode::setupNetCallbacks(leon::Engine& engine) {
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
        if (engine.GetGameInstance().IsListenServer() ||
            engine.GetGameInstance().IsDedicatedServer()) {
            spawnRemotePlayer(engine, playerSlotFromPeer(peerSlot));
            FillEmptySlotsWithBots(engine);
            NotifyClientsServerTravel(engine);
        }
    });
    net.SetOnPeerDisconnected([this, &engine](int peerSlot) {
        if (engine.GetGameInstance().IsListenServer() ||
            engine.GetGameInstance().IsDedicatedServer()) {
            const int playerSlot = playerSlotFromPeer(peerSlot);
            if (playerSlot >= 0 && playerSlot < leon::net::kMaxPlayers) {
                Logout(players_[static_cast<std::size_t>(playerSlot)]);
                remoteInputSeq_[static_cast<std::size_t>(playerSlot)] = 0;
                slotIsBot_[static_cast<std::size_t>(playerSlot)] = false;
                FillEmptySlotsWithBots(engine);
            }
            return;
        }
        EndMatch();
        Session(engine).ResetMatchTravelState();
        (void)ClientTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
    });
}

void FurytoonGameMode::OnEnter(leon::Engine& engine, const std::string& levelPath) {
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
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        pawnTargets_[static_cast<std::size_t>(i)] = {};
        remoteInputSeq_[static_cast<std::size_t>(i)] = 0;
        slotIsBot_[static_cast<std::size_t>(i)] = false;
    }
    GetWorld().Clear();
    GetGameState().Reset();
    hostKeyWasDown_ = dedicatedKeyWasDown_ = clientKeyWasDown_ = localhostKeyWasDown_ = false;
    escapeWasDown_ = pauseMenuOpen_ = bMatchOver_ = false;

    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    engine.GetAudioDevice().StopMusic();
    FurytoonGameInstance& session = Session(engine);
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
            spawnRemotePlayer(engine, playerSlotFromPeer(peer));
        }
        FillEmptySlotsWithBots(engine);
    } else if (engine.GetGameInstance().IsListenServer()) {
        spawnAllForListenServer(engine);
        const int peers = engine.GetGameInstance().GetNetDriver().PeerCount();
        for (int peer = 0; peer < peers; ++peer) {
            spawnRemotePlayer(engine, playerSlotFromPeer(peer));
        }
        FillEmptySlotsWithBots(engine);
    } else if (engine.GetGameInstance().IsClient()) {
        if (session.IsClientWelcomed()) {
            spawnAllForClient(engine, static_cast<std::uint8_t>(localSlot_));
        } else if (engine.GetGameInstance().GetNetDriver().IsConnected()) {
            leon::net::HelloMsg hello{};
            engine.GetGameInstance().GetNetDriver().SendToPeer(&hello, sizeof(hello), true);
        }
    } else {
        beginStandaloneMatch(engine);
    }

    engine.GetGameInstance().NotifyLevelOpened();
    engine.AddOnScreenDebugMessage("Furytoon — LMB light RMB heavy Space jump | Esc pause", 6.0f,
                                   {0.55f, 0.9f, 1.0f});
}

void FurytoonGameMode::OnExit(leon::Engine& engine) {
    EndMatch();
    logoutAllPlayers();
    leon::NetDriver& net = engine.GetGameInstance().GetNetDriver();
    net.SetOnPacket(nullptr);
    net.SetOnPeerConnected(nullptr);
    net.SetOnPeerDisconnected(nullptr);
    if (pauseMenu_ != nullptr) {
        engine.GetHUD().RemoveWidget(pauseMenu_);
        pauseMenu_ = nullptr;
    }
    pauseMenuOpen_ = false;
    GetWorld().Clear();
    engine_ = nullptr;
    engine.ClearCenterHudText();
    engine.SetKeyboardOrbitEnabled(true);
    engine.SetOrbitMouseEnabled(true);
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
}

void FurytoonGameMode::beginStandaloneMatch(leon::Engine& engine) {
    prepareMatchPhysics(engine);
    localSlot_ = 0;
    LoginPlayer(0, true);
    FillEmptySlotsWithBots(engine);
    StartMatch();
    MatchGameState().SetFightersAlive(CountLivingFighters());
    arenaCamera_ = {{0.0f, matchFloorY_ + 0.85f, 0.0f}, 18.0f};
    UpdateSharedArenaCamera(engine, 1.0f);
    EnableSharedArenaCamera(engine);
}

void FurytoonGameMode::beginDedicatedMatch(leon::Engine& engine) {
    destroyAllFighterMeshes(engine);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
        slotIsBot_[static_cast<std::size_t>(slot)] = false;
    }
    StartMatch();
    localSlot_ = -1;
    if (!engine.IsHeadless()) {
        arenaCamera_ = {{0.0f, matchFloorY_ + 0.5f, 0.0f}, 16.0f};
        UpdateSharedArenaCamera(engine, 1.0f);
        engine.SetKeyboardOrbitEnabled(true);
        engine.SetOrbitMouseEnabled(true);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
    }
}

void FurytoonGameMode::PostLogin(leon::PlayerController& newPlayer) {
    leon::GameMode::PostLogin(newPlayer);
    leon::PlayerState& ps = newPlayer.GetPlayerState();
    if (!ps.GetPlayerName().empty()) {
        return;
    }
    const int slot = slotOf(newPlayer);
    const bool dedicated = engine_ != nullptr && engine_->GetGameInstance().IsDedicatedServer();
    ps.SetPlayerName((slot == 0 && !dedicated) ? "Host" : ("Player " + std::to_string(slot)));
}

void FurytoonGameMode::Logout(leon::PlayerController& exiting) {
    const int slot = slotOf(exiting);
    if (slot >= 0 && slot < leon::net::kMaxPlayers) {
        players_[static_cast<std::size_t>(slot)].ClearCombatInput();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
        botControllers_[static_cast<std::size_t>(slot)].StopMovement();
        botControllers_[static_cast<std::size_t>(slot)].UnPossess();
    }
    exiting.UnPossess();
    if (slot >= 0 && slot < leon::net::kMaxPlayers) {
        if (FurytoonCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
            if (engine_ != nullptr) {
                existing->DestroyPrimitiveMeshes(engine_->GetLevel());
            }
            GetWorld().DestroyActor(existing);
            characters_[static_cast<std::size_t>(slot)] = nullptr;
        }
    }
    leon::GameMode::Logout(exiting);
}

void FurytoonGameMode::RestartPlayer(leon::PlayerController& newPlayer) {
    if (engine_ == nullptr) {
        return;
    }
    const int slot = slotOf(newPlayer);
    if (slot < 0 || slot >= leon::net::kMaxPlayers) {
        return;
    }
    newPlayer.UnPossess();
    botControllers_[static_cast<std::size_t>(slot)].UnPossess();
    if (FurytoonCharacter* existing = characters_[static_cast<std::size_t>(slot)]) {
        existing->DestroyPrimitiveMeshes(engine_->GetLevel());
        GetWorld().DestroyActor(existing);
        characters_[static_cast<std::size_t>(slot)] = nullptr;
    }

    auto* character = GetWorld().SpawnActor<FurytoonCharacter>();
    configurePawnForLevel(*character, engine_->GetLevel());
    EnsureFighterAccent(slot, *character);
    character->SetMaxHealth(FurytoonCharacter::kMaxHealth);
    character->Revive(FurytoonCharacter::kMaxHealth);

    const int startIdx = findPlayerStartIndex(engine_->GetLevel(), slot);
    glm::vec3 location{static_cast<float>(slot) * 2.0f, matchFloorY_, 0.0f};
    float yaw = static_cast<float>(slot) * 90.0f;
    if (startIdx >= 0) {
        const leon::PlayerStart& start =
            engine_->GetLevel().PlayerStarts()[static_cast<std::size_t>(startIdx)];
        location = start.transform.position;
        yaw = start.transform.rotationDegrees.y;
    }
    character->SpringArm().BoomYawDegrees = yaw;
    character->SpringArm().ClampPitch();
    snapPawnToFloor(*character, location);
    character->Reset(location, yaw + character->GetCharacterMovement().ModelYawOffsetDegrees);
    character->SyncTransformToLevel(engine_->GetLevel());
    character->EnsurePrimitiveMeshes(*engine_);
    character->SyncPrimitiveMeshes(engine_->GetLevel());
    characters_[static_cast<std::size_t>(slot)] = character;

    if (slotIsBot_[static_cast<std::size_t>(slot)]) {
        leon::AIController& ai = botControllers_[static_cast<std::size_t>(slot)];
        ai.SetNavigationSystem(&GetWorld().GetNavigationSystem());
        ai.SetArriveRadius(1.2f);
        ai.Possess(character);
    } else {
        newPlayer.Possess(character);
    }
}

void FurytoonGameMode::spawnAllForListenServer(leon::Engine& engine) {
    destroyAllFighterMeshes(engine);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
        slotIsBot_[static_cast<std::size_t>(slot)] = false;
    }
    localSlot_ = 0;
    LoginPlayer(0, true);
    FillEmptySlotsWithBots(engine);
    arenaCamera_ = {{0.0f, matchFloorY_ + 0.85f, 0.0f}, 18.0f};
    UpdateSharedArenaCamera(engine, 1.0f);
    StartMatch();
    MatchGameState().SetFightersAlive(CountLivingFighters());
    EnableSharedArenaCamera(engine);
}

void FurytoonGameMode::spawnAllForClient(leon::Engine& engine, std::uint8_t localSlot) {
    destroyAllFighterMeshes(engine);
    GetWorld().Clear();
    characters_.fill(nullptr);
    prepareMatchPhysics(engine);
    localSlot_ = static_cast<int>(localSlot);
    for (int slot = 0; slot < leon::net::kMaxPlayers; ++slot) {
        players_[static_cast<std::size_t>(slot)].UnPossess();
        remoteInputSeq_[static_cast<std::size_t>(slot)] = 0;
        slotIsBot_[static_cast<std::size_t>(slot)] = false;
        LoginPlayer(slot, slot == localSlot_);
    }
    StartMatch();
    Session(engine).SetClientWelcomed(true);
    arenaCamera_ = {{0.0f, matchFloorY_ + 0.85f, 0.0f}, 18.0f};
    UpdateSharedArenaCamera(engine, 1.0f);
    EnableSharedArenaCamera(engine);
}

void FurytoonGameMode::tryHost(leon::Engine& engine) {
    if (engine.GetGameInstance().IsListenServer()) {
        return;
    }
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().HostListen(leon::net::kDefaultPort)) {
        engine.AddOnScreenDebugMessage("Listen host failed", 3.0f, {1.0f, 0.3f, 0.3f});
        return;
    }
    setupNetCallbacks(engine);
    spawnAllForListenServer(engine);
}

void FurytoonGameMode::tryDedicated(leon::Engine& engine) {
    if (engine.GetGameInstance().IsDedicatedServer()) {
        engine.GetGameInstance().ClearPendingDedicatedStart();
        return;
    }
    const std::uint16_t port = engine.GetGameInstance().PendingDedicatedPort();
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().HostDedicated(port)) {
        engine.AddOnScreenDebugMessage("Dedicated failed", 4.0f, {1.0f, 0.3f, 0.3f});
        return;
    }
    engine.GetGameInstance().ClearPendingDedicatedStart();
    setupNetCallbacks(engine);
    beginDedicatedMatch(engine);
    FillEmptySlotsWithBots(engine);
}

void FurytoonGameMode::tryJoin(leon::Engine& engine, const std::string& address) {
    FurytoonGameInstance& session = Session(engine);
    session.SetJoinAddress(address.empty() ? "127.0.0.1" : address);
    engine.GetGameInstance().CloseNetSession();
    if (!engine.GetGameInstance().Join(session.GetJoinAddress(), leon::net::kDefaultPort)) {
        engine.AddOnScreenDebugMessage("Join failed", 3.0f, {1.0f, 0.3f, 0.3f});
        return;
    }
    setupNetCallbacks(engine);
    session.SetClientWelcomed(false);
    logoutAllPlayers();
    GetWorld().Clear();
    SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    RegisterBodiesFromLevel(engine.GetLevel());
    EndMatch();
}

void FurytoonGameMode::handlePacket(leon::Engine& engine, int peerSlot, const std::uint8_t* data,
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
            return;
        }
        // Dedicated: peer==slot. Listen: next free human among 1..3 (prefer peer+1).
        int assign = playerSlotFromPeer(peerSlot);
        if (!gi.IsDedicatedServer()) {
            const bool preferredFree =
                assign >= 1 && assign < leon::net::kMaxPlayers &&
                (characterAt(assign) == nullptr || slotIsBot_[static_cast<std::size_t>(assign)] ||
                 GetGameState().HasPlayerState(
                     &players_[static_cast<std::size_t>(assign)].GetPlayerState()));
            if (!preferredFree) {
                assign = -1;
                for (int s = 1; s < leon::net::kMaxPlayers; ++s) {
                    if (characterAt(s) == nullptr || slotIsBot_[static_cast<std::size_t>(s)]) {
                        assign = s;
                        break;
                    }
                }
            }
        }
        if (assign < 0 || assign >= leon::net::kMaxPlayers) {
            return;
        }
        leon::net::WelcomeMsg welcome{};
        welcome.slot = static_cast<std::uint8_t>(assign);
        leon::net::WriteLevelKey(welcome.levelKey, GetMapName());
        gi.GetNetDriver().SendToPeer(peerSlot, &welcome, sizeof(welcome), true);
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

void FurytoonGameMode::sendSnapshot(leon::Engine& engine) {
    leon::net::PawnSnap pawns[leon::net::kMaxSnapshotPawns]{};
    std::uint8_t pawnCount = 0;
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (FurytoonCharacter* ch = characterAt(i)) {
            FillPawnSnap(pawns[pawnCount++], static_cast<std::uint8_t>(i), *ch);
        }
    }
    leon::net::SnapshotMatchMeta meta{};
    meta.unitsAlive =
        static_cast<std::uint8_t>(std::clamp(MatchGameState().GetFightersAlive(), 0, 255));
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        const FurytoonPlayerState* ps =
            AsFurytoonPS(&players_[static_cast<std::size_t>(i)].GetPlayerState());
        if (ps == nullptr || !GetGameState().HasPlayerState(ps)) {
            continue;
        }
        meta.playerLives[i] = static_cast<std::uint8_t>(std::clamp(ps->GetStocks(), 0, 255));
        meta.playerElims[i] = static_cast<std::uint8_t>(std::clamp(ps->GetKOs(), 0, 255));
    }
    std::vector<std::uint8_t> packet;
    if (!leon::net::EncodeSnapshot(packet, GetGameState().GetReplicatedWorldTimeFrames(), pawns,
                                   pawnCount, nullptr, 0, &meta)) {
        return;
    }
    engine.GetGameInstance().GetNetDriver().Broadcast(packet.data(), packet.size(), false);
}

void FurytoonGameMode::applySnapshot(leon::Engine& engine, const std::uint8_t* data,
                                     std::size_t size) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }
    leon::net::DecodedSnapshot decoded;
    if (!leon::net::DecodeSnapshot(data, size, decoded)) {
        return;
    }
    GetGameState().SetReplicatedWorldTimeFrames(decoded.tick);
    MatchGameState().SetFightersAlive(decoded.UnitsAlive());
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        if (FurytoonPlayerState* ps =
                AsFurytoonPS(&players_[static_cast<std::size_t>(i)].GetPlayerState())) {
            ps->SetStocks(decoded.matchMeta.playerLives[i]);
            ps->SetKOs(decoded.matchMeta.playerElims[i]);
        }
    }
    for (const leon::net::PawnSnap& snap : decoded.pawns) {
        if (snap.slot >= leon::net::kMaxPlayers) {
            continue;
        }
        PawnTarget& t = pawnTargets_[snap.slot];
        t.pos = {snap.x, snap.y, snap.z};
        t.yaw = snap.yaw;
        t.velY = snap.velY;
        t.animBlend = snap.animBlend;
        t.boomYaw = snap.boomYaw;
        t.boomPitch = snap.boomPitch;
        t.health = snap.health;
        t.grounded = snap.grounded != 0;
        t.alive = (snap.flags & leon::net::kPawnSnapAlive) != 0;
        t.valid = true;
    }
}

void FurytoonGameMode::tickHost(leon::Engine& engine, float deltaTime) {
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        FurytoonCharacter* ch = characters_[static_cast<std::size_t>(i)];
        if (ch == nullptr) {
            continue;
        }
        if (!ch->IsAlive() || slotIsBot_[static_cast<std::size_t>(i)]) {
            if (!ch->IsAlive()) {
                ch->SetAnimBlendInput(0.0f);
            }
            continue;
        }
        if (pauseMenuOpen_ && i == localSlot_) {
            ch->SetAnimBlendInput(0.0f);
            continue;
        }
        const glm::vec3 move = players_[static_cast<std::size_t>(i)].TickInput(engine);
        ch->SetAnimBlendInput(ch->IsFalling() ? 0.0f : std::clamp(glm::length(move), 0.0f, 1.0f));
    }

    TickBotAI(deltaTime);
    ProcessCombat(engine, deltaTime);

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
    AppendFighterVisuals(engine);
    if (!pauseMenuOpen_) {
        UpdateSharedArenaCamera(engine, deltaTime);
    }

    GetGameState().IncrementReplicatedWorldTimeFrames();
    if (engine.GetGameInstance().GetNetDriver().HasPeer()) {
        sendSnapshot(engine);
    }
}

void FurytoonGameMode::tickClient(leon::Engine& engine, float deltaTime) {
    if (!Session(engine).IsClientWelcomed()) {
        return;
    }
    if (!pauseMenuOpen_ && characterAt(localSlot_) != nullptr) {
        players_[static_cast<std::size_t>(localSlot_)].TickInput(engine);
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].ConsumeLocalInputCmd();
        const bool reliable = cmd.jump != 0 ||
                              leon::net::HasInputButton(cmd, leon::net::InputButtons::Primary) ||
                              leon::net::HasInputButton(cmd, leon::net::InputButtons::Secondary);
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), reliable);
    } else if (characterAt(localSlot_) != nullptr) {
        const leon::net::InputCmdMsg cmd =
            players_[static_cast<std::size_t>(localSlot_)].MakeIdleInputCmd();
        engine.GetGameInstance().GetNetDriver().SendToPeer(&cmd, sizeof(cmd), false);
    }

    const float a = ExpAlpha(14.0f, deltaTime);
    for (int i = 0; i < leon::net::kMaxPlayers; ++i) {
        FurytoonCharacter* ch = characterAt(i);
        const PawnTarget& t = pawnTargets_[static_cast<std::size_t>(i)];
        if (ch == nullptr || !t.valid) {
            continue;
        }
        ch->ApplyReplicatedState(glm::mix(ch->GetActorLocation(), t.pos, a),
                                 LerpAngleDegrees(ch->GetActorYaw(), t.yaw, a), t.velY, t.grounded);
        ch->SetAnimBlendInput(t.animBlend);
        ch->SetHealth(t.health);
        if (!t.alive && ch->IsAlive()) {
            ch->SetHealth(0.0f);
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
    AppendFighterVisuals(engine);
    if (!pauseMenuOpen_) {
        UpdateSharedArenaCamera(engine, deltaTime);
    }
}

void FurytoonGameMode::updateHud(leon::Engine& engine) {
    if (engine.IsHeadless() || pauseMenuOpen_) {
        return;
    }
    if (engine.GetGameInstance().GetNetMode() == leon::ENetMode::DedicatedServer) {
        engine.SetCenterHudText("DEDICATED [" + GetMapName() + "]");
        return;
    }
    if (engine.GetGameInstance().IsClient() && !Session(engine).IsClientWelcomed()) {
        engine.SetCenterHudText("CLIENT joining...");
        return;
    }
    int stocks = 0;
    float hp = 0.0f;
    if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
        if (const FurytoonPlayerState* ps =
                AsFurytoonPS(&players_[static_cast<std::size_t>(localSlot_)].GetPlayerState())) {
            stocks = ps->GetStocks();
        }
        if (const FurytoonCharacter* ch = characterAt(localSlot_)) {
            hp = ch->GetHealth();
        }
    }
    std::string text =
        "Stocks " + std::to_string(stocks) + "  HP " + std::to_string(static_cast<int>(hp));
    if (bMatchOver_) {
        text += "\nMATCH OVER";
    }
    text += "\nLMB light RMB heavy Space jump";
    engine.SetCenterHudText(std::move(text));
}

void FurytoonGameMode::openPauseMenu(leon::Engine& engine) {
    pauseMenuOpen_ = true;
    if (localSlot_ >= 0 && localSlot_ < leon::net::kMaxPlayers) {
        (void)players_[static_cast<std::size_t>(localSlot_)].MakeIdleInputCmd();
    }
    engine.ClearCenterHudText();
    if (pauseMenu_ == nullptr) {
        pauseMenu_ = engine.GetHUD().AddWidget<leon::VerticalBoxWidget>();
    }
    pauseMenu_->SetVisibility(true);
    pauseMenu_->SetTitle("PAUSE");
    pauseMenu_->SetHint("Enter  |  Esc Resume");
    pauseMenu_->ClearChildren();
    pauseMenu_->AddButton("continue", "Resume");
    pauseMenu_->AddButton("menu", "Quit to Main Menu");
    pauseMenu_->SetSelectedIndex(0);
    pauseMenu_->ResetEdges();
    engine.SetPlayMouseLookActive(false);
    engine.SetCursorCaptured(false);
}

void FurytoonGameMode::closePauseMenu(leon::Engine& engine) {
    pauseMenuOpen_ = false;
    if (pauseMenu_ != nullptr) {
        pauseMenu_->SetVisibility(false);
    }
    engine.ClearCenterHudText();
    if (!engine.GetGameInstance().IsDedicatedServer()) {
        EnableSharedArenaCamera(engine);
    }
}

void FurytoonGameMode::returnToMenu(leon::Engine& engine) {
    pauseMenuOpen_ = false;
    if (pauseMenu_ != nullptr) {
        engine.GetHUD().RemoveWidget(pauseMenu_);
        pauseMenu_ = nullptr;
    }
    engine.GetGameInstance().CloseNetSession();
    Session(engine).ResetSession();
    EndMatch();
    if (engine.GetGameInstance().IsClient()) {
        (void)ClientTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
    } else {
        (void)ServerTravel(engine, FurytoonGameInstance::kMainMenuMap, levelPath_);
    }
}

void FurytoonGameMode::Tick(leon::Engine& engine, float deltaTime) {
    GetGameState().Tick(deltaTime);

    if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetPlayInputWindow();
        const bool pauseDown = window.IsKeyPressed(GLFW_KEY_ESCAPE) ||
                               window.IsKeyPressed(GLFW_KEY_BACKSPACE) ||
                               window.IsKeyPressed(GLFW_KEY_DELETE);
        if (pauseDown && !escapeWasDown_) {
            pauseMenuOpen_ ? closePauseMenu(engine) : openPauseMenu(engine);
        }
        escapeWasDown_ = pauseDown;
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
    if (!engine.IsHeadless()) {
        leon::Window& window = engine.GetWindow();
        const bool hostDown = window.IsKeyPressed(GLFW_KEY_H);
        const bool dedicatedDown = window.IsKeyPressed(GLFW_KEY_F9);
        const bool clientDown = window.IsKeyPressed(GLFW_KEY_C);
        const bool localhostDown = window.IsKeyPressed(GLFW_KEY_V);
        if (standalone && !pauseMenuOpen_) {
            if (hostDown && !hostKeyWasDown_) {
                tryHost(engine);
            }
            if (dedicatedDown && !dedicatedKeyWasDown_) {
                tryDedicated(engine);
            }
            if (clientDown && !clientKeyWasDown_) {
                tryJoin(engine, Session(engine).GetLanAddress().empty()
                                    ? "127.0.0.1"
                                    : Session(engine).GetLanAddress());
            }
            if (localhostDown && !localhostKeyWasDown_) {
                tryJoin(engine, "127.0.0.1");
            }
        }
        hostKeyWasDown_ = hostDown;
        dedicatedKeyWasDown_ = dedicatedDown;
        clientKeyWasDown_ = clientDown;
        localhostKeyWasDown_ = localhostDown;
    }

    engine.GetGameInstance().GetNetDriver().Poll();
    pauseMenuOpen_ ? engine.ClearCenterHudText() : updateHud(engine);

    if (!GetGameState().HasMatchStarted() && !engine.GetGameInstance().IsClient()) {
        return;
    }

    if (engine.GetGameInstance().IsNetHost() || standalone) {
        tickHost(engine, deltaTime);
    } else if (engine.GetGameInstance().IsClient()) {
        tickClient(engine, deltaTime);
    }
}

} // namespace game
