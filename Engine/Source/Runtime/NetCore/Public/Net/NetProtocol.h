#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Leon::Net
{

constexpr std::uint32_t kProtocolMagic = 0x4E4F454Cu; // 'LEON'
/// Bumped when fixed wire layouts change (Hello / InputCmd / FSnapshotHeader / Rpc).
constexpr std::uint16_t kProtocolVersion = 4;
constexpr int kDefaultPort = 7777;
constexpr int kMaxPlayers = 4;
/// Extra pawn slots in snapshots (AI / bots). Not connection slots — keep kMaxPlayers for peers.
constexpr int kMaxAiPawns = 16;
constexpr int kMaxSnapshotPawns = kMaxPlayers + kMaxAiPawns;
constexpr int kMaxDynamicBodies = 48;
/// Level identity in Welcome / Travel (LevelEntry.name or .llev stem).
constexpr std::size_t kMaxLevelKeyBytes = 64;
/// Stick move axes are expected in [-1, 1] after sanitize.
constexpr float kInputMoveAxisMax = 1.0f;
/// Host-side flood control (listen + dedicated). Sliding 1s window per peer.
constexpr int kMaxAcceptedPacketsPerPeerPerSecond = 240;
constexpr int kMaxRejectedPacketsPerPeerPerSecond = 64;

enum class ENetMsg : std::uint8_t {
    Hello = 1,
    Welcome = 2,
    InputCmd = 3,
    Snapshot = 4,
    /// Host → clients: switch to the named level (same catalog key as Welcome).
    Travel = 5,
    /// Reliable one-shot event (damage notify, interact, pack custom). Payload follows header.
    Rpc = 6,
};

/// Built-in RPC ids; packs may use custom ids from `kRpcIdPackBase` upward.
enum class ERpcId : std::uint8_t {
    None = 0,
    /// Generic reliable notify (payload optional UTF-8 / pack bytes).
    Notify = 1,
};

constexpr std::uint8_t kRpcIdPackBase = 16;
/// Max payload after FRpcHeader (keeps datagrams under typical MTU).
constexpr std::uint16_t kMaxRpcPayloadBytes = 512;

/// Game-agnostic action bits on FInputCmdMsg::buttons (packs assign meaning).
enum EInputButton : std::uint16_t {
    kInputButton0 = 1u << 0,
    kInputButton1 = 1u << 1,
    kInputButton2 = 1u << 2,
    kInputButton3 = 1u << 3,
    kInputButton4 = 1u << 4,
    kInputButton5 = 1u << 5,
    kInputButton6 = 1u << 6,
    kInputButton7 = 1u << 7,
    kInputButton8 = 1u << 8,
    kInputButton9 = 1u << 9,
    kInputButton10 = 1u << 10,
    kInputButton11 = 1u << 11,
    kInputButton12 = 1u << 12,
    kInputButton13 = 1u << 13,
    kInputButton14 = 1u << 14,
    kInputButton15 = 1u << 15,
};

/// All valid FInputCmdMsg::buttons bits (sanitize mask).
constexpr std::uint16_t kInputButtonMask = 0xFFFFu;

/// Suggested semantic aliases (packs may remap).
namespace InputButtons {
constexpr EInputButton Fire = kInputButton0;       // Zombies fire held
constexpr EInputButton Reload = kInputButton1;     // Zombies reload edge
constexpr EInputButton Use = kInputButton2;        // Zombies interact (doors / buys)
constexpr EInputButton Primary = kInputButton0;    // Furytoon light (shares slot 0)
constexpr EInputButton Secondary = kInputButton1;  // Furytoon heavy (shares slot 1)
constexpr EInputButton Scoreboard = kInputButton3; // hold to show scores
constexpr EInputButton Sprint = kInputButton4;     // hold to sprint
} // namespace InputButtons

#pragma pack(push, 1)

struct FHelloMsg {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::Hello);
    std::uint32_t magic = kProtocolMagic;
    std::uint16_t protocolVersion = kProtocolVersion;
};

struct FWelcomeMsg {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::Welcome);
    std::uint8_t slot = 0; // 0 = host local, 1 = joining client
    char levelKey[kMaxLevelKeyBytes]{};
};

struct FTravelMsg {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::Travel);
    std::uint8_t slot = 0; // client local slot after travel (host may send 0)
    char levelKey[kMaxLevelKeyBytes]{};
};

/// Fixed RPC framing. Wire: FRpcHeader | payload[payloadBytes].
struct FRpcHeader {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::Rpc);
    std::uint8_t rpcId = 0;
    std::uint8_t targetSlot = 0; // pawn / player slot the RPC addresses (pack-defined)
    std::uint8_t reserved = 0;
    std::uint16_t payloadBytes = 0;
};

/// Core locomotion + generic buttons. Pack-specific meaning lives in InputButtons aliases.
struct FInputCmdMsg {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::InputCmd);
    std::uint32_t seq = 0;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
    std::uint8_t jump = 0;
    std::uint16_t buttons = 0; // v4: 16 action bits (was uint8 in v3)
};

struct FPawnSnap {
    std::uint8_t slot = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    float velY = 0.0f;
    float animBlend = 0.0f;
    float boomYaw = 0.0f;
    float boomPitch = 0.0f;
    std::uint8_t grounded = 1;
    float health = 100.0f;
    /// bit0 = alive (1) / dead (0).
    std::uint8_t flags = 1;
    /// Pack-defined payload (e.g. ammo clip / reserve). Leave 0 when unused.
    std::uint8_t userByte0 = 0;
    std::uint16_t userWord0 = 0;
};

enum EPawnSnapFlags : std::uint8_t {
    kPawnSnapAlive = 1 << 0,
};

struct FBodySnap {
    std::uint32_t levelMeshIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
};

/// Fixed snapshot framing. Match / GameState fields are an optional extension blob.
struct FSnapshotHeader {
    std::uint8_t type = static_cast<std::uint8_t>(ENetMsg::Snapshot);
    std::uint32_t tick = 0;
    std::uint8_t pawnCount = 0;
    std::uint8_t bodyCount = 0;
    /// Bytes of extension immediately after this header (0 = none).
    std::uint16_t extBytes = 0;
};

/// Typed snapshot extension (extBytes == sizeof). Wire: Header | MatchMeta? | Pawns | Bodies.
struct FSnapshotMatchMeta {
    std::uint8_t roundIndex = 0;
    std::uint8_t unitsAlive = 0;
    std::uint8_t remainingSeconds = 0;
    std::uint16_t playerScore[kMaxPlayers]{};
    std::uint8_t playerLives[kMaxPlayers]{};
    std::uint8_t playerElims[kMaxPlayers]{};
};

#pragma pack(pop)

inline void WriteLevelKey(char (&dest)[kMaxLevelKeyBytes], std::string_view key) {
    std::memset(dest, 0, kMaxLevelKeyBytes);
    if (key.empty()) {
        return;
    }
    const std::size_t n =
        key.size() < (kMaxLevelKeyBytes - 1) ? key.size() : (kMaxLevelKeyBytes - 1);
    std::memcpy(dest, key.data(), n);
}

[[nodiscard]] inline std::string ReadLevelKey(const char (&src)[kMaxLevelKeyBytes]) {
    return std::string(src, strnlen(src, kMaxLevelKeyBytes));
}

[[nodiscard]] inline bool HasInputButton(const FInputCmdMsg& cmd, EInputButton button) {
    return (cmd.buttons & static_cast<std::uint16_t>(button)) != 0;
}

inline void SetInputButton(FInputCmdMsg& cmd, EInputButton button, bool down) {
    if (down) {
        cmd.buttons =
            static_cast<std::uint16_t>(cmd.buttons | static_cast<std::uint16_t>(button));
    } else {
        cmd.buttons = static_cast<std::uint16_t>(
            cmd.buttons & static_cast<std::uint16_t>(~static_cast<std::uint16_t>(button)));
    }
}

/// FPawnSnap user payload helpers (ammo-style packs).
inline void SetPawnUserAmmo(FPawnSnap& snap, std::uint8_t clip, std::uint16_t reserve) {
    snap.userByte0 = clip;
    snap.userWord0 = reserve;
}

inline void GetPawnUserAmmo(const FPawnSnap& snap, std::uint8_t& clip, std::uint16_t& reserve) {
    clip = snap.userByte0;
    reserve = snap.userWord0;
}

/// Minimum accepted datagram size for a known message type (0 = unknown / drop).
[[nodiscard]] inline std::size_t MinPacketSize(ENetMsg type) {
    switch (type) {
    case ENetMsg::Hello:
        return sizeof(FHelloMsg);
    case ENetMsg::Welcome:
        return sizeof(FWelcomeMsg);
    case ENetMsg::InputCmd:
        return sizeof(FInputCmdMsg);
    case ENetMsg::Snapshot:
        return sizeof(FSnapshotHeader);
    case ENetMsg::Travel:
        return sizeof(FTravelMsg);
    case ENetMsg::Rpc:
        return sizeof(FRpcHeader);
    default:
        return 0;
    }
}

[[nodiscard]] inline bool IsValidHello(const FHelloMsg& hello) {
    return hello.magic == kProtocolMagic && hello.protocolVersion == kProtocolVersion;
}

/// Flow: inbound datagram gate (NetDriver before onPacket_).
/// 1. Reject null / empty / unknown type
/// 2. Reject undersized fixed headers
/// 3. Hello: require magic + protocol version
[[nodiscard]] inline bool AcceptInboundPacket(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 1) {
        return false;
    }
    const auto type = static_cast<ENetMsg>(data[0]);
    const std::size_t minSize = MinPacketSize(type);
    if (minSize == 0 || size < minSize) {
        return false;
    }
    if (type == ENetMsg::Hello) {
        FHelloMsg hello{};
        std::memcpy(&hello, data, sizeof(hello));
        return IsValidHello(hello);
    }
    return true;
}

/// Clamp / finite-check remote InputCmd before applying to simulation.
inline void SanitizeInputCmd(FInputCmdMsg& cmd) {
    cmd.type = static_cast<std::uint8_t>(ENetMsg::InputCmd);
    auto sanitizeAxis = [](float v) -> float {
        if (!std::isfinite(v)) {
            return 0.0f;
        }
        return std::clamp(v, -kInputMoveAxisMax, kInputMoveAxisMax);
    };
    auto sanitizeAngle = [](float v) -> float {
        if (!std::isfinite(v)) {
            return 0.0f;
        }
        return v;
    };
    cmd.moveX = sanitizeAxis(cmd.moveX);
    cmd.moveZ = sanitizeAxis(cmd.moveZ);
    cmd.lookYaw = sanitizeAngle(cmd.lookYaw);
    cmd.lookPitch = sanitizeAngle(cmd.lookPitch);
    cmd.jump = cmd.jump != 0 ? 1 : 0;
    cmd.buttons = static_cast<std::uint16_t>(cmd.buttons & kInputButtonMask);
}

/// Encode FRpcHeader + optional payload into `out` (cleared first).
[[nodiscard]] inline bool EncodeRpc(std::vector<std::uint8_t>& out, ERpcId rpcId,
                                    std::uint8_t targetSlot, const void* payload,
                                    std::uint16_t payloadBytes) {
    if (payloadBytes > kMaxRpcPayloadBytes) {
        return false;
    }
    if (payloadBytes > 0 && payload == nullptr) {
        return false;
    }
    out.clear();
    out.resize(sizeof(FRpcHeader) + payloadBytes);
    FRpcHeader header{};
    header.rpcId = static_cast<std::uint8_t>(rpcId);
    header.targetSlot = targetSlot;
    header.payloadBytes = payloadBytes;
    std::memcpy(out.data(), &header, sizeof(header));
    if (payloadBytes > 0) {
        std::memcpy(out.data() + sizeof(FRpcHeader), payload, payloadBytes);
    }
    return true;
}

/// Decode FRpcHeader; `outPayload` points into `data` (not owned).
[[nodiscard]] inline bool DecodeRpc(const std::uint8_t* data, std::size_t size, FRpcHeader& outHeader,
                                    const std::uint8_t*& outPayload, std::uint16_t& outPayloadBytes) {
    outPayload = nullptr;
    outPayloadBytes = 0;
    if (data == nullptr || size < sizeof(FRpcHeader)) {
        return false;
    }
    std::memcpy(&outHeader, data, sizeof(outHeader));
    if (outHeader.type != static_cast<std::uint8_t>(ENetMsg::Rpc)) {
        return false;
    }
    if (outHeader.payloadBytes > kMaxRpcPayloadBytes) {
        return false;
    }
    if (size < sizeof(FRpcHeader) + outHeader.payloadBytes) {
        return false;
    }
    outPayloadBytes = outHeader.payloadBytes;
    outPayload = outPayloadBytes > 0 ? (data + sizeof(FRpcHeader)) : nullptr;
    return true;
}

/// Sliding 1-second packet window for host flood control.
struct FPeerPacketWindow {
    std::uint64_t windowStartMs = 0;
    int accepted = 0;
    int rejected = 0;
    bool active = false;

    enum class EAction : std::uint8_t {
        Allow = 0,
        Drop = 1,
        Disconnect = 2,
    };

    [[nodiscard]] EAction Observe(std::uint64_t nowMs, bool packetAccepted,
                                  int maxAcceptedPerSec = kMaxAcceptedPacketsPerPeerPerSecond,
                                  int maxRejectedPerSec = kMaxRejectedPacketsPerPeerPerSecond) {
        if (!active || nowMs < windowStartMs || (nowMs - windowStartMs) >= 1000) {
            windowStartMs = nowMs;
            accepted = 0;
            rejected = 0;
            active = true;
        }
        if (packetAccepted) {
            ++accepted;
            if (accepted > maxAcceptedPerSec) {
                return EAction::Disconnect;
            }
            return EAction::Allow;
        }
        ++rejected;
        if (rejected > maxRejectedPerSec) {
            return EAction::Disconnect;
        }
        return EAction::Drop;
    }

    void Reset() {
        windowStartMs = 0;
        accepted = 0;
        rejected = 0;
        active = false;
    }
};

} // namespace Leon::Net
