#include <cstring>
#include <leon/net/SnapshotCodec.h>

namespace leon {
namespace net {

bool EncodeSnapshot(std::vector<std::uint8_t>& outPacket, std::uint32_t tick, const PawnSnap* pawns,
                    std::uint8_t pawnCount, const BodySnap* bodies, std::uint8_t bodyCount,
                    const SnapshotMatchMeta* matchMeta) {
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
    SnapshotHeader header{};
    header.tick = tick;
    header.pawnCount = pawnCount;
    header.bodyCount = bodyCount;
    header.extBytes =
        matchMeta != nullptr ? static_cast<std::uint16_t>(sizeof(SnapshotMatchMeta)) : 0;

    const std::size_t bytes = sizeof(header) + header.extBytes + (sizeof(PawnSnap) * pawnCount) +
                              (sizeof(BodySnap) * bodyCount);
    outPacket.resize(bytes);

    std::size_t offset = 0;
    std::memcpy(outPacket.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    if (matchMeta != nullptr) {
        std::memcpy(outPacket.data() + offset, matchMeta, sizeof(SnapshotMatchMeta));
        offset += sizeof(SnapshotMatchMeta);
    }
    if (pawnCount > 0) {
        std::memcpy(outPacket.data() + offset, pawns, sizeof(PawnSnap) * pawnCount);
        offset += sizeof(PawnSnap) * pawnCount;
    }
    if (bodyCount > 0) {
        std::memcpy(outPacket.data() + offset, bodies, sizeof(BodySnap) * bodyCount);
    }
    return true;
}

bool DecodeSnapshot(const std::uint8_t* data, std::size_t size, DecodedSnapshot& out) {
    out = {};
    if (data == nullptr || size < sizeof(SnapshotHeader)) {
        return false;
    }

    SnapshotHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (header.type != static_cast<std::uint8_t>(ENetMsg::Snapshot)) {
        return false;
    }
    if (header.pawnCount > kMaxSnapshotPawns || header.bodyCount > kMaxDynamicBodies) {
        return false;
    }

    const std::size_t needed = sizeof(header) + header.extBytes +
                               (sizeof(PawnSnap) * header.pawnCount) +
                               (sizeof(BodySnap) * header.bodyCount);
    if (size < needed) {
        return false;
    }

    out.tick = header.tick;
    std::size_t offset = sizeof(header);
    if (header.extBytes == sizeof(SnapshotMatchMeta)) {
        std::memcpy(&out.matchMeta, data + offset, sizeof(SnapshotMatchMeta));
        out.hasMatchMeta = true;
        offset += sizeof(SnapshotMatchMeta);
    } else if (header.extBytes > 0) {
        // Unknown extension: skip so newer packs can add blobs without breaking older readers.
        offset += header.extBytes;
    }

    out.pawns.resize(header.pawnCount);
    out.bodies.resize(header.bodyCount);
    if (header.pawnCount > 0) {
        std::memcpy(out.pawns.data(), data + offset, sizeof(PawnSnap) * header.pawnCount);
        offset += sizeof(PawnSnap) * header.pawnCount;
    }
    if (header.bodyCount > 0) {
        std::memcpy(out.bodies.data(), data + offset, sizeof(BodySnap) * header.bodyCount);
    }
    return true;
}

} // namespace net
} // namespace leon
