#pragma once

#include <cstddef>
#include <cstdint>
#include <leon/net/NetProtocol.h>
#include <vector>

namespace leon {
namespace net {

/// Decoded authoritative snapshot (client interpolation targets).
struct DecodedSnapshot {
    std::uint32_t tick = 0;
    bool hasMatchMeta = false;
    SnapshotMatchMeta matchMeta{};
    std::vector<PawnSnap> pawns;
    std::vector<BodySnap> bodies;

    /// Convenience mirrors when hasMatchMeta (else 0).
    [[nodiscard]] std::uint8_t RoundIndex() const {
        return hasMatchMeta ? matchMeta.roundIndex : 0;
    }
    [[nodiscard]] std::uint8_t UnitsAlive() const {
        return hasMatchMeta ? matchMeta.unitsAlive : 0;
    }
    [[nodiscard]] std::uint8_t RemainingSeconds() const {
        return hasMatchMeta ? matchMeta.remainingSeconds : 0;
    }
};

/// Pack SnapshotHeader [| MatchMeta] | pawns | bodies.
/// When matchMeta is non-null, writes sizeof(SnapshotMatchMeta) extension after the header.
[[nodiscard]] bool EncodeSnapshot(std::vector<std::uint8_t>& outPacket, std::uint32_t tick,
                                  const PawnSnap* pawns, std::uint8_t pawnCount,
                                  const BodySnap* bodies, std::uint8_t bodyCount,
                                  const SnapshotMatchMeta* matchMeta = nullptr);

/// Parse a Snapshot datagram (validates sizes; unknown extBytes are skipped).
[[nodiscard]] bool DecodeSnapshot(const std::uint8_t* data, std::size_t size, DecodedSnapshot& out);

} // namespace net
} // namespace leon
