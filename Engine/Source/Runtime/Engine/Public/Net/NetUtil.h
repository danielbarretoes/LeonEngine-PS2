#pragma once

#include <string>
#include <string_view>


class UNetDriver;

namespace Leon::Net
{

/// Best-effort primary LAN IPv4 (skips 127.x). Empty if none found.
[[nodiscard]] std::string DetectPrimaryLanIPv4();

/// Host → remotes: reliable Travel (Unreal ClientTravel notify lite).
/// Dedicated: Travel.slot = peer index. Listen: Travel.slot = peer + 1 (matches pack
/// playerSlotFromPeer). No-op when not hosting or no peers.
void SendTravelToPeers(UNetDriver& net, std::string_view mapName, bool dedicatedServer);

} // namespace Leon::Net
