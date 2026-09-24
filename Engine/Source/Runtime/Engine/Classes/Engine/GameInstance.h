#pragma once

#include <cstdint>
#include <functional>
#include "Engine/NetDriver.h"
#include <memory>
#include <string>
#include <string_view>

namespace leon {

class Engine;

/// Persistent game session (Unreal-style `UGameInstance`). Survives level changes; owned by Engine.
/// Owns NetDriver for listen-server / dedicated / client LAN sessions.
class GameInstance {
public:
    /// Load a level by catalog key (name / stem). Wired by Runtime (LevelDirector) or Editor PIE.
    using LevelTravelFn = std::function<bool(Engine& engine, std::string_view levelKey)>;

    GameInstance();
    virtual ~GameInstance();

    GameInstance(const GameInstance&) = delete;
    GameInstance& operator=(const GameInstance&) = delete;
    GameInstance(GameInstance&&) = delete;
    GameInstance& operator=(GameInstance&&) = delete;

    virtual void Init();
    virtual void Shutdown();

    /// Called when a level is successfully activated for gameplay.
    virtual void NotifyLevelOpened() { ++levelsOpened_; }

    [[nodiscard]] int LevelsOpened() const { return levelsOpened_; }

    [[nodiscard]] NetDriver& GetNetDriver() { return *netDriver_; }
    [[nodiscard]] const NetDriver& GetNetDriver() const { return *netDriver_; }

    [[nodiscard]] ENetMode GetNetMode() const { return netDriver_->Mode(); }
    [[nodiscard]] bool IsListenServer() const { return GetNetMode() == ENetMode::ListenServer; }
    [[nodiscard]] bool IsDedicatedServer() const {
        return GetNetMode() == ENetMode::DedicatedServer;
    }
    [[nodiscard]] bool IsClient() const { return GetNetMode() == ENetMode::Client; }
    [[nodiscard]] bool IsNetHost() const { return netDriver_->IsHost(); }
    /// Unreal `HasAuthority` on the session (host / standalone).
    [[nodiscard]] bool HasAuthority() const { return GetNetMode() != ENetMode::Client; }

    /// Unreal-like Host / Join session (LAN).
    [[nodiscard]] bool HostListen(std::uint16_t port = net::kDefaultPort);
    [[nodiscard]] bool HostDedicated(std::uint16_t port = net::kDefaultPort);
    [[nodiscard]] bool Join(const std::string& address, std::uint16_t port = net::kDefaultPort);
    void CloseNetSession();

    /// Set by the app host (`--dedicated`) before a networked GameMode enters.
    void RequestDedicatedStart(std::uint16_t port = net::kDefaultPort) {
        pendingDedicatedStart_ = true;
        pendingDedicatedPort_ = port;
    }
    [[nodiscard]] bool HasPendingDedicatedStart() const { return pendingDedicatedStart_; }
    void ClearPendingDedicatedStart() { pendingDedicatedStart_ = false; }
    /// Clears and returns whether a dedicated start was pending (prefer Has + Clear after bind OK).
    [[nodiscard]] bool ConsumePendingDedicatedStart() {
        const bool pending = pendingDedicatedStart_;
        pendingDedicatedStart_ = false;
        return pending;
    }
    [[nodiscard]] std::uint16_t PendingDedicatedPort() const { return pendingDedicatedPort_; }

    /// Optional `--join <ip>` from the app host; Menu consumes into session address.
    void SetPendingJoinAddress(std::string address) { pendingJoinAddress_ = std::move(address); }
    [[nodiscard]] std::string ConsumePendingJoinAddress() {
        std::string address = std::move(pendingJoinAddress_);
        pendingJoinAddress_.clear();
        return address;
    }

    /// Optional `--listen` / `--host` from the app host; Menu auto HostListen → Lobby/map.
    void RequestListenStart(std::uint16_t port = net::kDefaultPort) {
        pendingListenStart_ = true;
        pendingListenPort_ = port;
    }
    [[nodiscard]] bool HasPendingListenStart() const { return pendingListenStart_; }
    [[nodiscard]] bool ConsumePendingListenStart() {
        const bool pending = pendingListenStart_;
        pendingListenStart_ = false;
        return pending;
    }
    [[nodiscard]] std::uint16_t PendingListenPort() const { return pendingListenPort_; }

    /// Optional `--map <LevelKey>` (Unreal Play current map): skip Lobby, travel to match.
    void SetPendingPlayMap(std::string mapKey) { pendingPlayMap_ = std::move(mapKey); }
    [[nodiscard]] std::string ConsumePendingPlayMap() {
        std::string map = std::move(pendingPlayMap_);
        pendingPlayMap_.clear();
        return map;
    }
    [[nodiscard]] const std::string& PeekPendingPlayMap() const { return pendingPlayMap_; }

    void SetLevelTravelFn(LevelTravelFn fn) { levelTravelFn_ = std::move(fn); }
    /// Used by Engine::SetGameInstance to keep Runtime/Editor travel wiring across subclass swap.
    [[nodiscard]] LevelTravelFn TakeLevelTravelFn() { return std::move(levelTravelFn_); }

    /// Toggle LevelDirector `[`/`]` chrome (menus hide it).
    using LevelBrowserVisibleFn = std::function<void(bool visible)>;
    void SetLevelBrowserVisibleFn(LevelBrowserVisibleFn fn) {
        levelBrowserVisibleFn_ = std::move(fn);
    }
    [[nodiscard]] LevelBrowserVisibleFn TakeLevelBrowserVisibleFn() {
        return std::move(levelBrowserVisibleFn_);
    }
    void SetLevelBrowserVisible(bool visible) {
        if (levelBrowserVisibleFn_) {
            levelBrowserVisibleFn_(visible);
        }
    }

    /// Unreal `UWorld::ServerTravel` — load map on authority / local process.
    [[nodiscard]] bool ServerTravel(Engine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});
    /// Unreal `APlayerController::ClientTravel` — load map on a client process.
    [[nodiscard]] bool ClientTravel(Engine& engine, std::string_view mapName,
                                    std::string_view hintLevelPath = {});

private:
    [[nodiscard]] bool TravelInternal(Engine& engine, std::string_view levelKey,
                                      std::string_view hintLevelPath);

    int levelsOpened_ = 0;
    bool pendingDedicatedStart_ = false;
    std::uint16_t pendingDedicatedPort_ = net::kDefaultPort;
    bool pendingListenStart_ = false;
    std::uint16_t pendingListenPort_ = net::kDefaultPort;
    std::string pendingJoinAddress_;
    std::string pendingPlayMap_;
    std::unique_ptr<NetDriver> netDriver_;
    LevelTravelFn levelTravelFn_;
    LevelBrowserVisibleFn levelBrowserVisibleFn_;
};

} // namespace leon
