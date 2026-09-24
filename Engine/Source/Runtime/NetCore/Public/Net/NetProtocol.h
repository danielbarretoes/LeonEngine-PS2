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

constexpr std::uint32_t ProtocolMagic = 0x4E4F454Cu; // 'LEON'
/// Bumped when fixed wire layouts change (Hello / InputCmd / FSnapshotHeader / Rpc).
constexpr std::uint16_t CurrentProtocolVersion = 4;
constexpr int DefaultPort = 7777;
constexpr int MaxPlayers = 4;
/// Extra pawn slots in snapshots (AI / bots). Not connection slots — keep kMaxPlayers for peers.
constexpr int MaxAiPawns = 16;
constexpr int MaxSnapshotPawns = MaxPlayers + MaxAiPawns;
constexpr int MaxDynamicBodies = 48;
/// Level identity in Welcome / Travel (FLevelEntry.name or .llev stem).
constexpr std::size_t MaxLevelKeyBytes = 64;
/// Stick move axes are expected in [-1, 1] after sanitize.
constexpr float InputMoveAxisMax = 1.0f;
/// Host-side flood control (listen + dedicated). Sliding 1s window per peer.
constexpr int MaxAcceptedPacketsPerPeerPerSecond = 240;
constexpr int MaxRejectedPacketsPerPeerPerSecond = 64;

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

constexpr std::uint8_t RpcIdPackBase = 16;
/// Max payload after FRpcHeader (keeps datagrams under typical MTU).
constexpr std::uint16_t MaxRpcPayloadBytes = 512;

/// Game-agnostic action bits on FInputCmdMsg::buttons (packs assign meaning).
enum EInputButton : std::uint16_t {
    InputButton0 = 1u << 0,
    InputButton1 = 1u << 1,
    InputButton2 = 1u << 2,
    InputButton3 = 1u << 3,
    InputButton4 = 1u << 4,
    InputButton5 = 1u << 5,
    InputButton6 = 1u << 6,
    InputButton7 = 1u << 7,
    InputButton8 = 1u << 8,
    InputButton9 = 1u << 9,
    InputButton10 = 1u << 10,
    InputButton11 = 1u << 11,
    InputButton12 = 1u << 12,
    InputButton13 = 1u << 13,
    InputButton14 = 1u << 14,
    InputButton15 = 1u << 15,
};

/// All valid FInputCmdMsg::buttons bits (sanitize mask).
constexpr std::uint16_t InputButtonMask = 0xFFFFu;

/// Suggested semantic aliases (packs may remap).
namespace InputButtons {
constexpr EInputButton Fire = InputButton0;       // Zombies fire held
constexpr EInputButton Reload = InputButton1;     // Zombies reload edge
constexpr EInputButton Use = InputButton2;        // Zombies interact (doors / buys)
constexpr EInputButton Primary = InputButton0;    // Furytoon light (shares slot 0)
constexpr EInputButton Secondary = InputButton1;  // Furytoon heavy (shares slot 1)
constexpr EInputButton Scoreboard = InputButton3; // hold to show scores
constexpr EInputButton Sprint = InputButton4;     // hold to sprint
} // namespace InputButtons

#pragma pack(push, 1)

struct FHelloMsg {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::Hello);
    std::uint32_t Magic = ProtocolMagic;
    std::uint16_t ProtocolVersion = CurrentProtocolVersion;
};

struct FWelcomeMsg {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::Welcome);
    std::uint8_t Slot = 0; // 0 = host local, 1 = joining client
    char LevelKey[MaxLevelKeyBytes]{};
};

struct FTravelMsg {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::Travel);
    std::uint8_t Slot = 0; // client local slot after travel (host may send 0)
    char LevelKey[MaxLevelKeyBytes]{};
};

/// Fixed RPC framing. Wire: FRpcHeader | payload[payloadBytes].
struct FRpcHeader {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::Rpc);
    std::uint8_t RpcId = 0;
    std::uint8_t TargetSlot = 0; // pawn / player slot the RPC addresses (pack-defined)
    std::uint8_t Reserved = 0;
    std::uint16_t PayloadBytes = 0;
};

/// Core locomotion + generic buttons. Pack-specific meaning lives in InputButtons aliases.
struct FInputCmdMsg {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::InputCmd);
    std::uint32_t Seq = 0;
    float MoveX = 0.0f;
    float MoveZ = 0.0f;
    float LookYaw = 0.0f;
    float LookPitch = 0.0f;
    std::uint8_t Jump = 0;
    std::uint16_t Buttons = 0; // v4: 16 action bits (was uint8 in v3)
};

struct FPawnSnap {
    std::uint8_t Slot = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Yaw = 0.0f;
    float VelY = 0.0f;
    float AnimBlend = 0.0f;
    float BoomYaw = 0.0f;
    float BoomPitch = 0.0f;
    std::uint8_t Grounded = 1;
    float Health = 100.0f;
    /// bit0 = alive (1) / dead (0).
    std::uint8_t Flags = 1;
    /// Pack-defined payload (e.g. ammo clip / reserve). Leave 0 when unused.
    std::uint8_t UserByte0 = 0;
    std::uint16_t UserWord0 = 0;
};

enum EPawnSnapFlags : std::uint8_t {
    PawnSnapAlive = 1 << 0,
};

struct FBodySnap {
    std::uint32_t LevelMeshIndex = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float VelX = 0.0f;
    float VelY = 0.0f;
    float VelZ = 0.0f;
};

/// Fixed snapshot framing. Match / GameState fields are an optional extension blob.
struct FSnapshotHeader {
    std::uint8_t Type = static_cast<std::uint8_t>(ENetMsg::Snapshot);
    std::uint32_t Tick = 0;
    std::uint8_t PawnCount = 0;
    std::uint8_t BodyCount = 0;
    /// Bytes of extension immediately after this header (0 = none).
    std::uint16_t ExtBytes = 0;
};

/// Typed snapshot extension (extBytes == sizeof). Wire: Header | MatchMeta? | Pawns | Bodies.
struct FSnapshotMatchMeta {
    std::uint8_t RoundIndex = 0;
    std::uint8_t UnitsAlive = 0;
    std::uint8_t RemainingSeconds = 0;
    std::uint16_t PlayerScore[MaxPlayers]{};
    std::uint8_t PlayerLives[MaxPlayers]{};
    std::uint8_t PlayerElims[MaxPlayers]{};
};

#pragma pack(pop)

inline void WriteLevelKey(char (&Dest)[MaxLevelKeyBytes], std::string_view Key) {
    std::memset(Dest, 0, MaxLevelKeyBytes);
    if (Key.empty()) {
        return;
    }
    const std::size_t N =
        Key.size() < (MaxLevelKeyBytes - 1) ? Key.size() : (MaxLevelKeyBytes - 1);
    std::memcpy(Dest, Key.data(), N);
}

[[nodiscard]] inline std::string ReadLevelKey(const char (&Src)[MaxLevelKeyBytes]) {
    return std::string(Src, strnlen(Src, MaxLevelKeyBytes));
}

[[nodiscard]] inline bool HasInputButton(const FInputCmdMsg& Cmd, EInputButton Button) {
    return (Cmd.Buttons & static_cast<std::uint16_t>(Button)) != 0;
}

inline void SetInputButton(FInputCmdMsg& Cmd, EInputButton Button, bool bDown) {
    if (bDown) {
        Cmd.Buttons =
            static_cast<std::uint16_t>(Cmd.Buttons | static_cast<std::uint16_t>(Button));
    } else {
        Cmd.Buttons = static_cast<std::uint16_t>(
            Cmd.Buttons & static_cast<std::uint16_t>(~static_cast<std::uint16_t>(Button)));
    }
}

/// FPawnSnap user payload helpers (ammo-style packs).
inline void SetPawnUserAmmo(FPawnSnap& Snap, std::uint8_t Clip, std::uint16_t Reserve) {
    Snap.UserByte0 = Clip;
    Snap.UserWord0 = Reserve;
}

inline void GetPawnUserAmmo(const FPawnSnap& Snap, std::uint8_t& Clip, std::uint16_t& Reserve) {
    Clip = Snap.UserByte0;
    Reserve = Snap.UserWord0;
}

/// Minimum accepted datagram size for a known message type (0 = unknown / drop).
[[nodiscard]] inline std::size_t MinPacketSize(ENetMsg InType) {
    switch (InType) {
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

[[nodiscard]] inline bool IsValidHello(const FHelloMsg& Hello) {
    return Hello.Magic == ProtocolMagic && Hello.ProtocolVersion == CurrentProtocolVersion;
}

/// Flow: inbound datagram gate (UNetDriver before onPacket_).
/// 1. Reject null / empty / unknown type
/// 2. Reject undersized fixed headers
/// 3. Hello: require magic + protocol version
[[nodiscard]] inline bool AcceptInboundPacket(const std::uint8_t* Data, std::size_t Size) {
    if (Data == nullptr || Size < 1) {
        return false;
    }
    const auto LocalType = static_cast<ENetMsg>(Data[0]);
    const std::size_t MinSize = MinPacketSize(LocalType);
    if (MinSize == 0 || Size < MinSize) {
        return false;
    }
    if (LocalType == ENetMsg::Hello) {
        FHelloMsg Hello{};
        std::memcpy(&Hello, Data, sizeof(Hello));
        return IsValidHello(Hello);
    }
    return true;
}

/// Clamp / finite-check remote InputCmd before applying to simulation.
inline void SanitizeInputCmd(FInputCmdMsg& Cmd) {
    Cmd.Type = static_cast<std::uint8_t>(ENetMsg::InputCmd);
    auto SanitizeAxis = [](float V) -> float {
        if (!std::isfinite(V)) {
            return 0.0f;
        }
        return std::clamp(V, -InputMoveAxisMax, InputMoveAxisMax);
    };
    auto SanitizeAngle = [](float V) -> float {
        if (!std::isfinite(V)) {
            return 0.0f;
        }
        return V;
    };
    Cmd.MoveX = SanitizeAxis(Cmd.MoveX);
    Cmd.MoveZ = SanitizeAxis(Cmd.MoveZ);
    Cmd.LookYaw = SanitizeAngle(Cmd.LookYaw);
    Cmd.LookPitch = SanitizeAngle(Cmd.LookPitch);
    Cmd.Jump = Cmd.Jump != 0 ? 1 : 0;
    Cmd.Buttons = static_cast<std::uint16_t>(Cmd.Buttons & InputButtonMask);
}

/// Encode FRpcHeader + optional payload into `out` (cleared first).
[[nodiscard]] inline bool EncodeRpc(std::vector<std::uint8_t>& Out, ERpcId InRpcId,
                                    std::uint8_t InTargetSlot, const void* Payload,
                                    std::uint16_t InPayloadBytes) {
    if (InPayloadBytes > MaxRpcPayloadBytes) {
        return false;
    }
    if (InPayloadBytes > 0 && Payload == nullptr) {
        return false;
    }
    Out.clear();
    Out.resize(sizeof(FRpcHeader) + InPayloadBytes);
    FRpcHeader Header{};
    Header.RpcId = static_cast<std::uint8_t>(InRpcId);
    Header.TargetSlot = InTargetSlot;
    Header.PayloadBytes = InPayloadBytes;
    std::memcpy(Out.data(), &Header, sizeof(Header));
    if (InPayloadBytes > 0) {
        std::memcpy(Out.data() + sizeof(FRpcHeader), Payload, InPayloadBytes);
    }
    return true;
}

/// Decode FRpcHeader; `outPayload` points into `data` (not owned).
[[nodiscard]] inline bool DecodeRpc(const std::uint8_t* Data, std::size_t Size, FRpcHeader& OutHeader,
                                    const std::uint8_t*& OutPayload, std::uint16_t& OutPayloadBytes) {
    OutPayload = nullptr;
    OutPayloadBytes = 0;
    if (Data == nullptr || Size < sizeof(FRpcHeader)) {
        return false;
    }
    std::memcpy(&OutHeader, Data, sizeof(OutHeader));
    if (OutHeader.Type != static_cast<std::uint8_t>(ENetMsg::Rpc)) {
        return false;
    }
    if (OutHeader.PayloadBytes > MaxRpcPayloadBytes) {
        return false;
    }
    if (Size < sizeof(FRpcHeader) + OutHeader.PayloadBytes) {
        return false;
    }
    OutPayloadBytes = OutHeader.PayloadBytes;
    OutPayload = OutPayloadBytes > 0 ? (Data + sizeof(FRpcHeader)) : nullptr;
    return true;
}

/// Sliding 1-second packet window for host flood control.
struct FPeerPacketWindow {
    std::uint64_t WindowStartMs = 0;
    int Accepted = 0;
    int Rejected = 0;
    bool bActive = false;

    enum class EAction : std::uint8_t {
        Allow = 0,
        Drop = 1,
        Disconnect = 2,
    };

    [[nodiscard]] EAction Observe(std::uint64_t NowMs, bool bPacketAccepted,
                                  int MaxAcceptedPerSec = MaxAcceptedPacketsPerPeerPerSecond,
                                  int MaxRejectedPerSec = MaxRejectedPacketsPerPeerPerSecond) {
        if (!bActive || NowMs < WindowStartMs || (NowMs - WindowStartMs) >= 1000) {
            WindowStartMs = NowMs;
            Accepted = 0;
            Rejected = 0;
            bActive = true;
        }
        if (bPacketAccepted) {
            ++Accepted;
            if (Accepted > MaxAcceptedPerSec) {
                return EAction::Disconnect;
            }
            return EAction::Allow;
        }
        ++Rejected;
        if (Rejected > MaxRejectedPerSec) {
            return EAction::Disconnect;
        }
        return EAction::Drop;
    }

    void Reset() {
        WindowStartMs = 0;
        Accepted = 0;
        Rejected = 0;
        bActive = false;
    }
};

} // namespace Leon::Net
