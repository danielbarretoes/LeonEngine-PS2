#include <cstring>
#include "Net/SnapshotCodec.h"

namespace Leon::Net
{

bool EncodeSnapshot(std::vector<std::uint8_t>& outPacket, std::uint32_t tick, const FPawnSnap* pawns,
                    std::uint8_t pawnCount, const FBodySnap* bodies, std::uint8_t bodyCount,
                    const FSnapshotMatchMeta* matchMeta) {
    if (pawnCount > kMaxSnapshotPawns) {
        return false;
    }
    if (bodyCount > kMaxDynamicBodies) {
        return false;
    }
    if ((pawnCount > 0 && pawns == nullptr) || (bodyCount > 0 && bodies == nullptr)) {
        return false;
    }

    // Flow: Header → optional MatchMeta ext → pawns → bodies.
    FSnapshotHeader header{};
    header.tick = tick;
    header.pawnCount = pawnCount;
    header.bodyCount = bodyCount;
    header.extBytes =
        matchMeta != nullptr ? static_cast<std::uint16_t>(sizeof(FSnapshotMatchMeta)) : 0;

    const std::size_t bytes = sizeof(header) + header.extBytes + (sizeof(FPawnSnap) * pawnCount) +
                              (sizeof(FBodySnap) * bodyCount);
    outPacket.resize(bytes);

    std::size_t offset = 0;
    std::memcpy(outPacket.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    if (matchMeta != nullptr) {
        std::memcpy(outPacket.data() + offset, matchMeta, sizeof(FSnapshotMatchMeta));
        offset += sizeof(FSnapshotMatchMeta);
    }
    if (pawnCount > 0) {
        std::memcpy(outPacket.data() + offset, pawns, sizeof(FPawnSnap) * pawnCount);
        offset += sizeof(FPawnSnap) * pawnCount;
    }
    if (bodyCount > 0) {
        std::memcpy(outPacket.data() + offset, bodies, sizeof(FBodySnap) * bodyCount);
    }
    return true;
}

bool DecodeSnapshot(const std::uint8_t* data, std::size_t size, FDecodedSnapshot& out) {
    out = {};
    if (data == nullptr || size < sizeof(FSnapshotHeader)) {
        return false;
    }

    FSnapshotHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (header.type != static_cast<std::uint8_t>(ENetMsg::Snapshot)) {
        return false;
    }
    if (header.pawnCount > kMaxSnapshotPawns || header.bodyCount > kMaxDynamicBodies) {
        return false;
    }

    const std::size_t needed = sizeof(header) + header.extBytes +
                               (sizeof(FPawnSnap) * header.pawnCount) +
                               (sizeof(FBodySnap) * header.bodyCount);
    if (size < needed) {
        return false;
    }

    out.tick = header.tick;
    std::size_t offset = sizeof(header);
    if (header.extBytes == sizeof(FSnapshotMatchMeta)) {
        std::memcpy(&out.matchMeta, data + offset, sizeof(FSnapshotMatchMeta));
        out.hasMatchMeta = true;
        offset += sizeof(FSnapshotMatchMeta);
    } else if (header.extBytes > 0) {
        // Unknown extension: skip so newer packs can add blobs without breaking older readers.
        offset += header.extBytes;
    }

    out.pawns.resize(header.pawnCount);
    out.bodies.resize(header.bodyCount);
    if (header.pawnCount > 0) {
        std::memcpy(out.pawns.data(), data + offset, sizeof(FPawnSnap) * header.pawnCount);
        offset += sizeof(FPawnSnap) * header.pawnCount;
    }
    if (header.bodyCount > 0) {
        std::memcpy(out.bodies.data(), data + offset, sizeof(FBodySnap) * header.bodyCount);
    }
    return true;
}

} // namespace Leon::Net
