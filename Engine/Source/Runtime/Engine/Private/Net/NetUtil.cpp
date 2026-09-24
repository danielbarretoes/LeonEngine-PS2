#include "Net/NetUtil.h"

#include "Engine/NetDriver.h"
#include "Net/NetProtocol.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

namespace Leon::Net
{
	namespace
	{

		bool EnsureSockets()
		{
#if defined(_WIN32)
			static bool bReady = false;
			if (bReady)
			{
				return true;
			}
			WSADATA Data{};
			if (WSAStartup(MAKEWORD(2, 2), &Data) != 0)
			{
				return false;
			}
			bReady = true;
			return true;
#else
			return true;
#endif
		}

	} // namespace

	std::string DetectPrimaryLanIPv4()
	{
		if (!EnsureSockets())
		{
			return {};
		}

		char Hostname[256]{};
#if defined(_WIN32)
		if (gethostname(Hostname, static_cast<int>(sizeof(Hostname))) != 0)
		{
#else
		if (gethostname(hostname, sizeof(hostname)) != 0)
		{
#endif
			return {};
		}

		addrinfo Hints{};
		Hints.ai_family = AF_INET;
		Hints.ai_socktype = SOCK_STREAM;

		addrinfo* Result = nullptr;
		if (getaddrinfo(Hostname, nullptr, &Hints, &Result) != 0 || Result == nullptr)
		{
			return {};
		}

		std::string Chosen;
		for (addrinfo* P = Result; P != nullptr; P = P->ai_next)
		{
			if (P->ai_addr == nullptr)
			{
				continue;
			}
			auto* Sa = reinterpret_cast<sockaddr_in*>(P->ai_addr);
			char Buf[INET_ADDRSTRLEN]{};
			if (inet_ntop(AF_INET, &Sa->sin_addr, Buf, sizeof(Buf)) == nullptr)
			{
				continue;
			}
			const std::string Candidate = Buf;
			if (Candidate.rfind("127.", 0) == 0)
			{
				continue;
			}
			Chosen = Candidate;
			break;
		}
		freeaddrinfo(Result);
		return Chosen;
	}

	void SendTravelToPeers(UNetDriver& Net, std::string_view MapName, bool bDedicatedServer)
	{
		if (!Net.IsHost() || !Net.HasPeer())
		{
			return;
		}
		const int Peers = Net.PeerCount();
		for (int Peer = 0; Peer < Peers; ++Peer)
		{
			FTravelMsg Travel{};
			Travel.Slot = bDedicatedServer ? static_cast<std::uint8_t>(Peer) : static_cast<std::uint8_t>(Peer + 1);
			WriteLevelKey(Travel.LevelKey, MapName);
			Net.SendToPeer(Peer, &Travel, sizeof(Travel), true);
		}
		std::cout << "NetTravel: Travel -> peers map='" << MapName << "'\n";
	}

} // namespace Leon::Net
