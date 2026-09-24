#include <algorithm>
#include <chrono>
#include <cstdint>
#include <enet/enet.h>
#include <iostream>
#include "Engine/NetDriver.h"

namespace {

/// Process-wide ENet init refcount — deinitialize only when the last NetDriver drops.
int g_enetInitCount = 0;

[[nodiscard]] std::uint64_t steadyNowMs() {
    using clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch())
            .count());
}

} // namespace


NetDriver::~NetDriver() {
    Shutdown();
    if (libraryReady_) {
        libraryReady_ = false;
        if (g_enetInitCount > 0) {
            --g_enetInitCount;
        }
        if (g_enetInitCount == 0) {
            enet_deinitialize();
        }
    }
}

bool NetDriver::ensureInitialized() {
    if (libraryReady_) {
        return true;
    }
    if (g_enetInitCount == 0) {
        if (enet_initialize() != 0) {
            std::cerr << "NetDriver: enet_initialize failed\n";
            return false;
        }
    }
    ++g_enetInitCount;
    libraryReady_ = true;
    return true;
}

int NetDriver::PeerCount() const {
    int count = 0;
    for (int i = 0; i < maxClients_; ++i) {
        if (peers_[static_cast<std::size_t>(i)] != nullptr) {
            ++count;
        }
    }
    return count;
}

int NetDriver::allocatePeerSlot(ENetPeer* peer) {
    if (peer == nullptr) {
        return -1;
    }
    for (int i = 0; i < maxClients_; ++i) {
        if (peers_[static_cast<std::size_t>(i)] == nullptr) {
            peers_[static_cast<std::size_t>(i)] = peer;
            peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(i));
            return i;
        }
    }
    return -1;
}

void NetDriver::clearPeerSlot(ENetPeer* peer) {
    if (peer == nullptr) {
        return;
    }
    const auto slot = static_cast<int>(reinterpret_cast<std::intptr_t>(peer->data));
    if (slot >= 0 && slot < Leon::Net::kMaxPlayers && peers_[static_cast<std::size_t>(slot)] == peer) {
        peers_[static_cast<std::size_t>(slot)] = nullptr;
        peerRates_[static_cast<std::size_t>(slot)].Reset();
    }
    peer->data = nullptr;
}

void NetDriver::disconnectPeerForAbuse(int peerSlot) {
    if (peerSlot < 0 || peerSlot >= Leon::Net::kMaxPlayers) {
        return;
    }
    ENetPeer* peer = peers_[static_cast<std::size_t>(peerSlot)];
    if (peer == nullptr) {
        return;
    }
    std::cerr << "NetDriver: disconnecting peer slot " << peerSlot << " (rate limit)\n";
    enet_peer_disconnect_now(peer, 0);
    clearPeerSlot(peer);
    if (onPeerDisconnected_) {
        onPeerDisconnected_(peerSlot);
    }
}

bool NetDriver::startServer(std::uint16_t port, int maxClients, ENetMode mode) {
    Shutdown();
    if (!ensureInitialized()) {
        return false;
    }
    if (maxClients < 1) {
        maxClients = 1;
    }
    if (maxClients > Leon::Net::kMaxPlayers) {
        maxClients = Leon::Net::kMaxPlayers;
    }

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = port;

    host_ = enet_host_create(&address, static_cast<std::size_t>(maxClients), 2, 0, 0);
    if (host_ == nullptr) {
        std::cerr << "NetDriver: failed to create host on port " << port << '\n';
        return false;
    }
    maxClients_ = maxClients;
    peers_.fill(nullptr);
    for (auto& rate : peerRates_) {
        rate.Reset();
    }
    mode_ = mode;
    connected_ = true;
    const char* label = (mode == ENetMode::DedicatedServer) ? "DedicatedServer" : "ListenServer";
    std::cout << "NetDriver: " << label << " on 0.0.0.0:" << port << " (maxClients=" << maxClients_
              << ")\n";
    return true;
}

bool NetDriver::StartHost(std::uint16_t port) {
    // Listen host: local player occupies one player slot; remotes fill the rest.
    return startServer(port, std::max(1, Leon::Net::kMaxPlayers - 1), ENetMode::ListenServer);
}

bool NetDriver::StartDedicated(std::uint16_t port) {
    return startServer(port, Leon::Net::kMaxPlayers, ENetMode::DedicatedServer);
}

bool NetDriver::Connect(const std::string& address, std::uint16_t port) {
    Shutdown();
    if (!ensureInitialized()) {
        return false;
    }

    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (host_ == nullptr) {
        std::cerr << "NetDriver: failed to create client host\n";
        return false;
    }

    ENetAddress addr{};
    if (enet_address_set_host(&addr, address.c_str()) != 0) {
        std::cerr << "NetDriver: failed to resolve host '" << address << "'\n";
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }
    addr.port = port;

    ENetPeer* peer = enet_host_connect(host_, &addr, 2, 0);
    if (peer == nullptr) {
        std::cerr << "NetDriver: connect failed\n";
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    maxClients_ = 1;
    peers_.fill(nullptr);
    peers_[0] = peer;
    peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(0));
    for (auto& rate : peerRates_) {
        rate.Reset();
    }
    mode_ = ENetMode::Client;
    connected_ = false;
    std::cout << "NetDriver: connecting to " << address << ':' << port << '\n';
    return true;
}

void NetDriver::Shutdown() {
    if (host_ != nullptr) {
        for (int i = 0; i < Leon::Net::kMaxPlayers; ++i) {
            ENetPeer*& peer = peers_[static_cast<std::size_t>(i)];
            if (peer != nullptr) {
                enet_peer_disconnect_now(peer, 0);
                peer = nullptr;
            }
            peerRates_[static_cast<std::size_t>(i)].Reset();
        }
        enet_host_destroy(host_);
        host_ = nullptr;
    }
    maxClients_ = 1;
    mode_ = ENetMode::Standalone;
    connected_ = false;
}

void NetDriver::Poll() {
    if (host_ == nullptr) {
        return;
    }

    ENetEvent event{};
    while (enet_host_service(host_, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            if (mode_ == ENetMode::Client) {
                peers_[0] = event.peer;
                if (event.peer != nullptr) {
                    event.peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(0));
                }
                connected_ = true;
                std::cout << "NetDriver: connected to server\n";
                if (onPeerConnected_) {
                    onPeerConnected_(0);
                }
                break;
            }

            const int slot = allocatePeerSlot(event.peer);
            if (slot < 0) {
                std::cerr << "NetDriver: rejecting peer (server full)\n";
                if (event.peer != nullptr) {
                    enet_peer_disconnect_now(event.peer, 0);
                }
                break;
            }
            peerRates_[static_cast<std::size_t>(slot)].Reset();
            std::cout << "NetDriver: peer connected (slot " << slot << ")\n";
            if (onPeerConnected_) {
                onPeerConnected_(slot);
            }
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
            if (event.packet != nullptr) {
                // Flow: host rate window → AcceptInboundPacket → onPacket_ (or drop / kick).
                bool destroyPacket = true;
                if (event.packet->data != nullptr) {
                    const std::size_t size = event.packet->dataLength;
                    const std::uint8_t* data = event.packet->data;
                    int slot = 0;
                    if (event.peer != nullptr) {
                        slot = static_cast<int>(reinterpret_cast<std::intptr_t>(event.peer->data));
                    }
                    const bool accepted = Leon::Net::AcceptInboundPacket(data, size);
                    bool deliver = accepted;
                    if (IsHost() && peerRateLimitEnabled_ && slot >= 0 && slot < Leon::Net::kMaxPlayers) {
                        const auto action = peerRates_[static_cast<std::size_t>(slot)].Observe(
                            steadyNowMs(), accepted);
                        if (action == Leon::Net::PeerPacketWindow::EAction::Disconnect) {
                            deliver = false;
                            enet_packet_destroy(event.packet);
                            destroyPacket = false;
                            disconnectPeerForAbuse(slot);
                        } else if (action == Leon::Net::PeerPacketWindow::EAction::Drop) {
                            deliver = false;
                        }
                    }
                    if (deliver && onPacket_) {
                        onPacket_(slot, data, size);
                    }
                }
                if (destroyPacket) {
                    enet_packet_destroy(event.packet);
                }
            }
            break;
        case ENET_EVENT_TYPE_DISCONNECT: {
            int slot = 0;
            if (event.peer != nullptr) {
                slot = static_cast<int>(reinterpret_cast<std::intptr_t>(event.peer->data));
            }
            std::cout << "NetDriver: peer disconnected (slot " << slot << ")\n";
            clearPeerSlot(event.peer);
            if (mode_ == ENetMode::Client) {
                connected_ = false;
            }
            if (onPeerDisconnected_) {
                onPeerDisconnected_(slot);
            }
            break;
        }
        default:
            break;
        }
    }
}

void NetDriver::SendToPeer(int peerSlot, const void* data, std::size_t size, bool reliable) {
    if (data == nullptr || size == 0 || peerSlot < 0 || peerSlot >= Leon::Net::kMaxPlayers) {
        return;
    }
    ENetPeer* peer = peers_[static_cast<std::size_t>(peerSlot)];
    if (peer == nullptr) {
        return;
    }
    const enet_uint32 flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (packet == nullptr) {
        return;
    }
    enet_peer_send(peer, 0, packet);
    enet_host_flush(host_);
}

void NetDriver::Broadcast(const void* data, std::size_t size, bool reliable) {
    if (host_ == nullptr || data == nullptr || size == 0) {
        return;
    }
    const enet_uint32 flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (packet == nullptr) {
        return;
    }
    enet_host_broadcast(host_, 0, packet);
    enet_host_flush(host_);
}

