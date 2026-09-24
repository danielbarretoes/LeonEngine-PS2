#pragma once

#include "Net/NetProtocol.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>

struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

enum class ENetMode : std::uint8_t
{
	Standalone = 0,
	ListenServer = 1,
	Client = 2,
	DedicatedServer = 3,
};

/// Thin ENet wrapper (Unreal-like UNetDriver / UNetDriver micro).
class ENGINE_API UNetDriver
{
public:
	/// peerSlot: host remote index [0, maxClients), or 0 for the server when Client.
	using FPacketHandler = std::function<void(int PeerSlot, const std::uint8_t* Data, std::size_t Size)>;
	using FPeerHandler = std::function<void(int PeerSlot)>;

	UNetDriver() = default;
	~UNetDriver();

	UNetDriver(const UNetDriver&) = delete;
	UNetDriver& operator=(const UNetDriver&) = delete;

	/// Listen-server: remotes fill fighter slots after the local host player.
	[[nodiscard]] bool StartHost(std::uint16_t Port = Leon::Net::DefaultPort);
	/// Dedicated: up to MaxPlayers remote clients, no local player.
	[[nodiscard]] bool StartDedicated(std::uint16_t Port = Leon::Net::DefaultPort);
	[[nodiscard]] bool Connect(const std::string& Address, std::uint16_t Port = Leon::Net::DefaultPort);
	void Shutdown();

	void Poll();

	/// Client → server, or host → specific remote peerSlot.
	void SendToPeer(int PeerSlot, const void* Data, std::size_t Size, bool bReliable = false);
	/// Convenience: client uses peerSlot 0 (the server).
	void SendToPeer(const void* Data, std::size_t Size, bool bReliable = false)
	{
		SendToPeer(0, Data, Size, bReliable);
	}
	/// Broadcast to all connected remotes (host / dedicated).
	void Broadcast(const void* Data, std::size_t Size, bool bReliable = false);

	[[nodiscard]] ENetMode GetMode() const
	{
		return Mode;
	}
	[[nodiscard]] bool IsHost() const
	{
		return Mode == ENetMode::ListenServer || Mode == ENetMode::DedicatedServer;
	}
	[[nodiscard]] bool IsListenServer() const
	{
		return Mode == ENetMode::ListenServer;
	}
	[[nodiscard]] bool IsDedicatedServer() const
	{
		return Mode == ENetMode::DedicatedServer;
	}
	[[nodiscard]] bool IsClient() const
	{
		return Mode == ENetMode::Client;
	}
	[[nodiscard]] int PeerCount() const;
	[[nodiscard]] bool HasPeer() const
	{
		return PeerCount() > 0;
	}
	[[nodiscard]] bool IsConnected() const
	{
		return bConnected;
	}
	[[nodiscard]] int GetMaxClients() const
	{
		return MaxClients;
	}

	/// When true (default), host drops/disconnects peers that exceed packet windows.
	void SetPeerRateLimitEnabled(bool bEnabled)
	{
		bPeerRateLimitEnabled = bEnabled;
	}
	[[nodiscard]] bool IsPeerRateLimitEnabled() const
	{
		return bPeerRateLimitEnabled;
	}

	void SetOnPacket(FPacketHandler Handler)
	{
		OnPacket = std::move(Handler);
	}
	void SetOnPeerConnected(FPeerHandler Handler)
	{
		OnPeerConnected = std::move(Handler);
	}
	void SetOnPeerDisconnected(FPeerHandler Handler)
	{
		OnPeerDisconnected = std::move(Handler);
	}

private:
	bool EnsureInitialized();
	[[nodiscard]] bool StartServer(std::uint16_t Port, int InMaxClients, ENetMode InMode);
	[[nodiscard]] int AllocatePeerSlot(ENetPeer* Peer);
	void ClearPeerSlot(ENetPeer* Peer);
	void DisconnectPeerForAbuse(int PeerSlot);

	ENetHost* Host = nullptr;
	std::array<ENetPeer*, Leon::Net::MaxPlayers> Peers{};
	std::array<Leon::Net::FPeerPacketWindow, Leon::Net::MaxPlayers> PeerRates{};
	int MaxClients = 1;
	ENetMode Mode = ENetMode::Standalone;
	/// True when this instance holds a process-wide ENet init ref.
	bool bLibraryReady = false;
	bool bConnected = false;
	bool bPeerRateLimitEnabled = true;
	FPacketHandler OnPacket;
	FPeerHandler OnPeerConnected;
	FPeerHandler OnPeerDisconnected;
};
