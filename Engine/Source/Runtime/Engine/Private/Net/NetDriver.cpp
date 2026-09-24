#include <algorithm>
#include <chrono>
#include <cstdint>
#include <enet/enet.h>
#include <iostream>
#include "Engine/NetDriver.h"

namespace {

/// Process-wide ENet init refcount — deinitialize only when the last UNetDriver drops.
int GEnetInitCount = 0;

[[nodiscard]] std::uint64_t SteadyNowMs() {
    using clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch())
            .count());
}

} // namespace


UNetDriver::~UNetDriver() {
    Shutdown();
    if (bLibraryReady) {
        bLibraryReady = false;
        if (GEnetInitCount > 0) {
            --GEnetInitCount;
        }
        if (GEnetInitCount == 0) {
            enet_deinitialize();
        }
    }
}

bool UNetDriver::EnsureInitialized() {
    if (bLibraryReady) {
        return true;
    }
    if (GEnetInitCount == 0) {
        if (enet_initialize() != 0) {
            std::cerr << "NetDriver: enet_initialize failed\n";
            return false;
        }
    }
    ++GEnetInitCount;
    bLibraryReady = true;
    return true;
}

int UNetDriver::PeerCount() const {
    int Count = 0;
    for (int I = 0; I < MaxClients; ++I) {
        if (Peers[static_cast<std::size_t>(I)] != nullptr) {
            ++Count;
        }
    }
    return Count;
}

int UNetDriver::AllocatePeerSlot(ENetPeer* Peer) {
    if (Peer == nullptr) {
        return -1;
    }
    for (int I = 0; I < MaxClients; ++I) {
        if (Peers[static_cast<std::size_t>(I)] == nullptr) {
            Peers[static_cast<std::size_t>(I)] = Peer;
            Peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(I));
            return I;
        }
    }
    return -1;
}

void UNetDriver::ClearPeerSlot(ENetPeer* Peer) {
    if (Peer == nullptr) {
        return;
    }
    const auto Slot = static_cast<int>(reinterpret_cast<std::intptr_t>(Peer->data));
    if (Slot >= 0 && Slot < Leon::Net::MaxPlayers && Peers[static_cast<std::size_t>(Slot)] == Peer) {
        Peers[static_cast<std::size_t>(Slot)] = nullptr;
        PeerRates[static_cast<std::size_t>(Slot)].Reset();
    }
    Peer->data = nullptr;
}

void UNetDriver::DisconnectPeerForAbuse(int PeerSlot) {
    if (PeerSlot < 0 || PeerSlot >= Leon::Net::MaxPlayers) {
        return;
    }
    ENetPeer* Peer = Peers[static_cast<std::size_t>(PeerSlot)];
    if (Peer == nullptr) {
        return;
    }
    std::cerr << "NetDriver: disconnecting peer slot " << PeerSlot << " (rate limit)\n";
    enet_peer_disconnect_now(Peer, 0);
    ClearPeerSlot(Peer);
    if (OnPeerDisconnected) {
        OnPeerDisconnected(PeerSlot);
    }
}

bool UNetDriver::StartServer(std::uint16_t Port, int InMaxClients, ENetMode InMode) {
    Shutdown();
    if (!EnsureInitialized()) {
        return false;
    }
    if (InMaxClients < 1) {
        InMaxClients = 1;
    }
    if (InMaxClients > Leon::Net::MaxPlayers) {
        InMaxClients = Leon::Net::MaxPlayers;
    }

    ENetAddress Address{};
    Address.host = ENET_HOST_ANY;
    Address.port = Port;

    Host = enet_host_create(&Address, static_cast<std::size_t>(InMaxClients), 2, 0, 0);
    if (Host == nullptr) {
        std::cerr << "NetDriver: failed to create host on port " << Port << '\n';
        return false;
    }
    MaxClients = InMaxClients;
    Peers.fill(nullptr);
    for (auto& Rate : PeerRates) {
        Rate.Reset();
    }
    Mode = InMode;
    bConnected = true;
    const char* Label = (InMode == ENetMode::DedicatedServer) ? "DedicatedServer" : "ListenServer";
    std::cout << "NetDriver: " << Label << " on 0.0.0.0:" << Port << " (maxClients=" << MaxClients
              << ")\n";
    return true;
}

bool UNetDriver::StartHost(std::uint16_t Port) {
    // Listen host: local player occupies one player slot; remotes fill the rest.
    return StartServer(Port, std::max(1, Leon::Net::MaxPlayers - 1), ENetMode::ListenServer);
}

bool UNetDriver::StartDedicated(std::uint16_t Port) {
    return StartServer(Port, Leon::Net::MaxPlayers, ENetMode::DedicatedServer);
}

bool UNetDriver::Connect(const std::string& Address, std::uint16_t Port) {
    Shutdown();
    if (!EnsureInitialized()) {
        return false;
    }

    Host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (Host == nullptr) {
        std::cerr << "NetDriver: failed to create client host\n";
        return false;
    }

    ENetAddress Addr{};
    if (enet_address_set_host(&Addr, Address.c_str()) != 0) {
        std::cerr << "NetDriver: failed to resolve host '" << Address << "'\n";
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }
    Addr.port = Port;

    ENetPeer* Peer = enet_host_connect(Host, &Addr, 2, 0);
    if (Peer == nullptr) {
        std::cerr << "NetDriver: connect failed\n";
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }

    MaxClients = 1;
    Peers.fill(nullptr);
    Peers[0] = Peer;
    Peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(0));
    for (auto& Rate : PeerRates) {
        Rate.Reset();
    }
    Mode = ENetMode::Client;
    bConnected = false;
    std::cout << "NetDriver: connecting to " << Address << ':' << Port << '\n';
    return true;
}

void UNetDriver::Shutdown() {
    if (Host != nullptr) {
        for (int I = 0; I < Leon::Net::MaxPlayers; ++I) {
            ENetPeer*& Peer = Peers[static_cast<std::size_t>(I)];
            if (Peer != nullptr) {
                enet_peer_disconnect_now(Peer, 0);
                Peer = nullptr;
            }
            PeerRates[static_cast<std::size_t>(I)].Reset();
        }
        enet_host_destroy(Host);
        Host = nullptr;
    }
    MaxClients = 1;
    Mode = ENetMode::Standalone;
    bConnected = false;
}

void UNetDriver::Poll() {
    if (Host == nullptr) {
        return;
    }

    ENetEvent Event{};
    while (enet_host_service(Host, &Event, 0) > 0) {
        switch (Event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            if (Mode == ENetMode::Client) {
                Peers[0] = Event.peer;
                if (Event.peer != nullptr) {
                    Event.peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(0));
                }
                bConnected = true;
                std::cout << "NetDriver: connected to server\n";
                if (OnPeerConnected) {
                    OnPeerConnected(0);
                }
                break;
            }

            const int Slot = AllocatePeerSlot(Event.peer);
            if (Slot < 0) {
                std::cerr << "NetDriver: rejecting peer (server full)\n";
                if (Event.peer != nullptr) {
                    enet_peer_disconnect_now(Event.peer, 0);
                }
                break;
            }
            PeerRates[static_cast<std::size_t>(Slot)].Reset();
            std::cout << "NetDriver: peer connected (slot " << Slot << ")\n";
            if (OnPeerConnected) {
                OnPeerConnected(Slot);
            }
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
            if (Event.packet != nullptr) {
                // Flow: host rate window → AcceptInboundPacket → OnPacket (or drop / kick).
                bool bDestroyPacket = true;
                if (Event.packet->data != nullptr) {
                    const std::size_t Size = Event.packet->dataLength;
                    const std::uint8_t* Data = Event.packet->data;
                    int Slot = 0;
                    if (Event.peer != nullptr) {
                        Slot = static_cast<int>(reinterpret_cast<std::intptr_t>(Event.peer->data));
                    }
                    const bool bAccepted = Leon::Net::AcceptInboundPacket(Data, Size);
                    bool bDeliver = bAccepted;
                    if (IsHost() && bPeerRateLimitEnabled && Slot >= 0 && Slot < Leon::Net::MaxPlayers) {
                        const auto Action = PeerRates[static_cast<std::size_t>(Slot)].Observe(
                            SteadyNowMs(), bAccepted);
                        if (Action == Leon::Net::FPeerPacketWindow::EAction::Disconnect) {
                            bDeliver = false;
                            enet_packet_destroy(Event.packet);
                            bDestroyPacket = false;
                            DisconnectPeerForAbuse(Slot);
                        } else if (Action == Leon::Net::FPeerPacketWindow::EAction::Drop) {
                            bDeliver = false;
                        }
                    }
                    if (bDeliver && OnPacket) {
                        OnPacket(Slot, Data, Size);
                    }
                }
                if (bDestroyPacket) {
                    enet_packet_destroy(Event.packet);
                }
            }
            break;
        case ENET_EVENT_TYPE_DISCONNECT: {
            int Slot = 0;
            if (Event.peer != nullptr) {
                Slot = static_cast<int>(reinterpret_cast<std::intptr_t>(Event.peer->data));
            }
            std::cout << "NetDriver: peer disconnected (slot " << Slot << ")\n";
            ClearPeerSlot(Event.peer);
            if (Mode == ENetMode::Client) {
                bConnected = false;
            }
            if (OnPeerDisconnected) {
                OnPeerDisconnected(Slot);
            }
            break;
        }
        default:
            break;
        }
    }
}

void UNetDriver::SendToPeer(int PeerSlot, const void* Data, std::size_t Size, bool bReliable) {
    if (Data == nullptr || Size == 0 || PeerSlot < 0 || PeerSlot >= Leon::Net::MaxPlayers) {
        return;
    }
    ENetPeer* Peer = Peers[static_cast<std::size_t>(PeerSlot)];
    if (Peer == nullptr) {
        return;
    }
    const enet_uint32 Flags = bReliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* Packet = enet_packet_create(Data, Size, Flags);
    if (Packet == nullptr) {
        return;
    }
    enet_peer_send(Peer, 0, Packet);
    enet_host_flush(Host);
}

void UNetDriver::Broadcast(const void* Data, std::size_t Size, bool bReliable) {
    if (Host == nullptr || Data == nullptr || Size == 0) {
        return;
    }
    const enet_uint32 Flags = bReliable ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* Packet = enet_packet_create(Data, Size, Flags);
    if (Packet == nullptr) {
        return;
    }
    enet_host_broadcast(Host, 0, Packet);
    enet_host_flush(Host);
}

