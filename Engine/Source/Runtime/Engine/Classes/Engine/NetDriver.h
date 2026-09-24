#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include "Net/NetProtocol.h"
#include <string>

struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;


enum class ENetMode : std::uint8_t {
    Standalone = 0,
    ListenServer = 1,
    Client = 2,
    DedicatedServer = 3,
};

/// Thin ENet wrapper (Unreal-like UNetDriver / UNetDriver micro).
class UNetDriver {
public:
    /// peerSlot: host remote index [0, maxClients), or 0 for the server when Client.
    using FPacketHandler =
        std::function<void(int peerSlot, const std::uint8_t* data, std::size_t size)>;
    using FPeerHandler = std::function<void(int peerSlot)>;

    UNetDriver() = default;
    ~UNetDriver();

    UNetDriver(const UNetDriver&) = delete;
    UNetDriver& operator=(const UNetDriver&) = delete;

    /// Listen-server: remotes fill fighter slots after the local host player.
    [[nodiscard]] bool StartHost(std::uint16_t port = Leon::Net::kDefaultPort);
    /// Dedicated: up to kMaxPlayers remote clients, no local player.
    [[nodiscard]] bool StartDedicated(std::uint16_t port = Leon::Net::kDefaultPort);
    [[nodiscard]] bool Connect(const std::string& address, std::uint16_t port = Leon::Net::kDefaultPort);
    void Shutdown();

    void Poll();

    /// Client → server, or host → specific remote peerSlot.
    void SendToPeer(int peerSlot, const void* data, std::size_t size, bool reliable = false);
    /// Convenience: client uses peerSlot 0 (the server).
    void SendToPeer(const void* data, std::size_t size, bool reliable = false) {
        SendToPeer(0, data, size, reliable);
    }
    /// Broadcast to all connected remotes (host / dedicated).
    void Broadcast(const void* data, std::size_t size, bool reliable = false);

    [[nodiscard]] ENetMode Mode() const { return mode_; }
    [[nodiscard]] bool IsHost() const {
        return mode_ == ENetMode::ListenServer || mode_ == ENetMode::DedicatedServer;
    }
    [[nodiscard]] bool IsListenServer() const { return mode_ == ENetMode::ListenServer; }
    [[nodiscard]] bool IsDedicatedServer() const { return mode_ == ENetMode::DedicatedServer; }
    [[nodiscard]] bool IsClient() const { return mode_ == ENetMode::Client; }
    [[nodiscard]] int PeerCount() const;
    [[nodiscard]] bool HasPeer() const { return PeerCount() > 0; }
    [[nodiscard]] bool IsConnected() const { return connected_; }
    [[nodiscard]] int MaxClients() const { return maxClients_; }

    /// When true (default), host drops/disconnects peers that exceed packet windows.
    void SetPeerRateLimitEnabled(bool enabled) { peerRateLimitEnabled_ = enabled; }
    [[nodiscard]] bool IsPeerRateLimitEnabled() const { return peerRateLimitEnabled_; }

    void SetOnPacket(FPacketHandler handler) { onPacket_ = std::move(handler); }
    void SetOnPeerConnected(FPeerHandler handler) { onPeerConnected_ = std::move(handler); }
    void SetOnPeerDisconnected(FPeerHandler handler) { onPeerDisconnected_ = std::move(handler); }

private:
    bool ensureInitialized();
    [[nodiscard]] bool startServer(std::uint16_t port, int maxClients, ENetMode mode);
    [[nodiscard]] int allocatePeerSlot(ENetPeer* peer);
    void clearPeerSlot(ENetPeer* peer);
    void disconnectPeerForAbuse(int peerSlot);

    ENetHost* host_ = nullptr;
    std::array<ENetPeer*, Leon::Net::kMaxPlayers> peers_{};
    std::array<Leon::Net::FPeerPacketWindow, Leon::Net::kMaxPlayers> peerRates_{};
    int maxClients_ = 1;
    ENetMode mode_ = ENetMode::Standalone;
    /// True when this instance holds a process-wide ENet init ref.
    bool libraryReady_ = false;
    bool connected_ = false;
    bool peerRateLimitEnabled_ = true;
    FPacketHandler onPacket_;
    FPeerHandler onPeerConnected_;
    FPeerHandler onPeerDisconnected_;
};

