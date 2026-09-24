#pragma once

#include <cstdint>
#include <functional>
#include "Engine/NetDriver.h"
#include <memory>
#include <string>
#include <string_view>


class UGameEngine;

/// Persistent game session (Unreal-style `UGameInstance`). Survives level changes; owned by Engine.
/// Owns UNetDriver for listen-server / dedicated / client LAN sessions.
class UGameInstance {
public:
    /// Load a level by catalog key (name / stem). Wired by Runtime (FLevelDirector) or Editor PIE.
    using FLevelTravelFunction = std::function<bool(UGameEngine& Engine, std::string_view LevelKey)>;

    UGameInstance();
    virtual ~UGameInstance();

    UGameInstance(const UGameInstance&) = delete;
    UGameInstance& operator=(const UGameInstance&) = delete;
    UGameInstance(UGameInstance&&) = delete;
    UGameInstance& operator=(UGameInstance&&) = delete;

    virtual void Init();
    virtual void Shutdown();

    /// Called when a level is successfully activated for gameplay.
    virtual void NotifyLevelOpened() { ++LevelsOpened; }

    [[nodiscard]] int GetLevelsOpened() const { return LevelsOpened; }

    [[nodiscard]] UNetDriver& GetNetDriver() { return *NetDriver; }
    [[nodiscard]] const UNetDriver& GetNetDriver() const { return *NetDriver; }

    [[nodiscard]] ENetMode GetNetMode() const { return NetDriver->GetMode(); }
    [[nodiscard]] bool IsListenServer() const { return GetNetMode() == ENetMode::ListenServer; }
    [[nodiscard]] bool IsDedicatedServer() const {
        return GetNetMode() == ENetMode::DedicatedServer;
    }
    [[nodiscard]] bool IsClient() const { return GetNetMode() == ENetMode::Client; }
    [[nodiscard]] bool IsNetHost() const { return NetDriver->IsHost(); }
    /// Unreal `HasAuthority` on the session (host / standalone).
    [[nodiscard]] bool HasAuthority() const { return GetNetMode() != ENetMode::Client; }

    /// Unreal-like Host / Join session (LAN).
    [[nodiscard]] bool HostListen(std::uint16_t Port = Leon::Net::DefaultPort);
    [[nodiscard]] bool HostDedicated(std::uint16_t Port = Leon::Net::DefaultPort);
    [[nodiscard]] bool Join(const std::string& Address, std::uint16_t Port = Leon::Net::DefaultPort);
    void CloseNetSession();

    /// Set by the app host (`--dedicated`) before a networked GameMode enters.
    void RequestDedicatedStart(std::uint16_t Port = Leon::Net::DefaultPort) {
        bPendingDedicatedStart = true;
        PendingDedicatedPort = Port;
    }
    [[nodiscard]] bool HasPendingDedicatedStart() const { return bPendingDedicatedStart; }
    void ClearPendingDedicatedStart() { bPendingDedicatedStart = false; }
    /// Clears and returns whether a dedicated start was pending (prefer Has + Clear after bind OK).
    [[nodiscard]] bool ConsumePendingDedicatedStart() {
        const bool bPending = bPendingDedicatedStart;
        bPendingDedicatedStart = false;
        return bPending;
    }
    [[nodiscard]] std::uint16_t GetPendingDedicatedPort() const { return PendingDedicatedPort; }

    /// Optional `--join <ip>` from the app host; Menu consumes into session address.
    void SetPendingJoinAddress(std::string Address) { PendingJoinAddress = std::move(Address); }
    [[nodiscard]] std::string ConsumePendingJoinAddress() {
        std::string Address = std::move(PendingJoinAddress);
        PendingJoinAddress.clear();
        return Address;
    }

    /// Optional `--listen` / `--host` from the app host; Menu auto HostListen → Lobby/map.
    void RequestListenStart(std::uint16_t Port = Leon::Net::DefaultPort) {
        bPendingListenStart = true;
        PendingListenPort = Port;
    }
    [[nodiscard]] bool HasPendingListenStart() const { return bPendingListenStart; }
    [[nodiscard]] bool ConsumePendingListenStart() {
        const bool bPending = bPendingListenStart;
        bPendingListenStart = false;
        return bPending;
    }
    [[nodiscard]] std::uint16_t GetPendingListenPort() const { return PendingListenPort; }

    /// Optional `--map <LevelKey>` (Unreal Play current map): skip Lobby, travel to match.
    void SetPendingPlayMap(std::string MapKey) { PendingPlayMap = std::move(MapKey); }
    [[nodiscard]] std::string ConsumePendingPlayMap() {
        std::string Map = std::move(PendingPlayMap);
        PendingPlayMap.clear();
        return Map;
    }
    [[nodiscard]] const std::string& PeekPendingPlayMap() const { return PendingPlayMap; }

    void SetLevelTravelFn(FLevelTravelFunction Fn) { LevelTravelFn = std::move(Fn); }
    /// Used by Engine::SetGameInstance to keep Runtime/Editor travel wiring across subclass swap.
    [[nodiscard]] FLevelTravelFunction TakeLevelTravelFn() { return std::move(LevelTravelFn); }

    /// Toggle FLevelDirector `[`/`]` chrome (menus hide it).
    using FLevelBrowserVisibleFunction = std::function<void(bool bVisible)>;
    void SetLevelBrowserVisibleFn(FLevelBrowserVisibleFunction Fn) {
        LevelBrowserVisibleFn = std::move(Fn);
    }
    [[nodiscard]] FLevelBrowserVisibleFunction TakeLevelBrowserVisibleFn() {
        return std::move(LevelBrowserVisibleFn);
    }
    void SetLevelBrowserVisible(bool bVisible) {
        if (LevelBrowserVisibleFn) {
            LevelBrowserVisibleFn(bVisible);
        }
    }

    /// Unreal `UWorld::ServerTravel` — load map on authority / local process.
    [[nodiscard]] bool ServerTravel(UGameEngine& Engine, std::string_view MapName,
                                    std::string_view HintLevelPath = {});
    /// Unreal `APlayerController::ClientTravel` — load map on a client process.
    [[nodiscard]] bool ClientTravel(UGameEngine& Engine, std::string_view MapName,
                                    std::string_view HintLevelPath = {});

private:
    [[nodiscard]] bool TravelInternal(UGameEngine& Engine, std::string_view LevelKey,
                                      std::string_view HintLevelPath);

    int LevelsOpened = 0;
    bool bPendingDedicatedStart = false;
    std::uint16_t PendingDedicatedPort = Leon::Net::DefaultPort;
    bool bPendingListenStart = false;
    std::uint16_t PendingListenPort = Leon::Net::DefaultPort;
    std::string PendingJoinAddress;
    std::string PendingPlayMap;
    std::unique_ptr<UNetDriver> NetDriver;
    FLevelTravelFunction LevelTravelFn;
    FLevelBrowserVisibleFunction LevelBrowserVisibleFn;
};

