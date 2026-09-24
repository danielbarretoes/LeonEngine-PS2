#include <cstdint>
#include <cstring>
#include <iostream>
#include "Engine/NetDriver.h"
#include "Net/NetProtocol.h"
#include "Net/NetUtil.h"
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
namespace {

bool ensureSockets() {
#if defined(_WIN32)
    static bool ready = false;
    if (ready) {
        return true;
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
    ready = true;
    return true;
#else
    return true;
#endif
}

} // namespace

std::string DetectPrimaryLanIPv4() {
    if (!ensureSockets()) {
        return {};
    }

    char hostname[256]{};
#if defined(_WIN32)
    if (gethostname(hostname, static_cast<int>(sizeof(hostname))) != 0) {
#else
    if (gethostname(hostname, sizeof(hostname)) != 0) {
#endif
        return {};
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0 || result == nullptr) {
        return {};
    }

    std::string chosen;
    for (addrinfo* p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_addr == nullptr) {
            continue;
        }
        auto* sa = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        char buf[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)) == nullptr) {
            continue;
        }
        const std::string candidate = buf;
        if (candidate.rfind("127.", 0) == 0) {
            continue;
        }
        chosen = candidate;
        break;
    }
    freeaddrinfo(result);
    return chosen;
}

void SendTravelToPeers(UNetDriver& net, std::string_view mapName, bool dedicatedServer) {
    if (!net.IsHost() || !net.HasPeer()) {
        return;
    }
    const int peers = net.PeerCount();
    for (int peer = 0; peer < peers; ++peer) {
        FTravelMsg travel{};
        travel.slot =
            dedicatedServer ? static_cast<std::uint8_t>(peer) : static_cast<std::uint8_t>(peer + 1);
        WriteLevelKey(travel.levelKey, mapName);
        net.SendToPeer(peer, &travel, sizeof(travel), true);
    }
    std::cout << "NetTravel: Travel -> peers map='" << mapName << "'\n";
}

} // namespace Leon::Net
