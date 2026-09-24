#pragma once

#include <cstddef>
#include <cstdint>
#include "Net/NetProtocol.h"
#include <vector>

namespace Leon::Net
{

/// Decoded authoritative snapshot (client interpolation targets).
struct FDecodedSnapshot {
    std::uint32_t tick = 0;
    bool hasMatchMeta = false;
    FSnapshotMatchMeta matchMeta{};
    std::vector<FPawnSnap> pawns;
    std::vector<FBodySnap> bodies;

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

/// Pack FSnapshotHeader [| MatchMeta] | pawns | bodies.
/// When matchMeta is non-null, writes sizeof(FSnapshotMatchMeta) extension after the header.
[[nodiscard]] bool EncodeSnapshot(std::vector<std::uint8_t>& outPacket, std::uint32_t tick,
                                  const FPawnSnap* pawns, std::uint8_t pawnCount,
                                  const FBodySnap* bodies, std::uint8_t bodyCount,
                                  const FSnapshotMatchMeta* matchMeta = nullptr);

/// Parse a Snapshot datagram (validates sizes; unknown extBytes are skipped).
[[nodiscard]] bool DecodeSnapshot(const std::uint8_t* data, std::size_t size, FDecodedSnapshot& out);

} // namespace Leon::Net
