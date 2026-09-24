#include "Net/SnapshotCodec.h"

#include <cstring>

namespace Leon::Net
{

	bool EncodeSnapshot(std::vector<std::uint8_t>& OutPacket, std::uint32_t InTick, const FPawnSnap* InPawns,
		std::uint8_t PawnCount, const FBodySnap* InBodies, std::uint8_t BodyCount,
		const FSnapshotMatchMeta* InMatchMeta)
	{
		if (PawnCount > MaxSnapshotPawns)
		{
			return false;
		}
		if (BodyCount > MaxDynamicBodies)
		{
			return false;
		}
		if ((PawnCount > 0 && InPawns == nullptr) || (BodyCount > 0 && InBodies == nullptr))
		{
			return false;
		}

		// Flow: Header → optional MatchMeta ext → pawns → bodies.
		FSnapshotHeader Header{};
		Header.Tick = InTick;
		Header.PawnCount = PawnCount;
		Header.BodyCount = BodyCount;
		Header.ExtBytes = InMatchMeta != nullptr ? static_cast<std::uint16_t>(sizeof(FSnapshotMatchMeta)) : 0;

		const std::size_t Bytes =
			sizeof(Header) + Header.ExtBytes + (sizeof(FPawnSnap) * PawnCount) + (sizeof(FBodySnap) * BodyCount);
		OutPacket.resize(Bytes);

		std::size_t Offset = 0;
		std::memcpy(OutPacket.data() + Offset, &Header, sizeof(Header));
		Offset += sizeof(Header);
		if (InMatchMeta != nullptr)
		{
			std::memcpy(OutPacket.data() + Offset, InMatchMeta, sizeof(FSnapshotMatchMeta));
			Offset += sizeof(FSnapshotMatchMeta);
		}
		if (PawnCount > 0)
		{
			std::memcpy(OutPacket.data() + Offset, InPawns, sizeof(FPawnSnap) * PawnCount);
			Offset += sizeof(FPawnSnap) * PawnCount;
		}
		if (BodyCount > 0)
		{
			std::memcpy(OutPacket.data() + Offset, InBodies, sizeof(FBodySnap) * BodyCount);
		}
		return true;
	}

	bool DecodeSnapshot(const std::uint8_t* Data, std::size_t Size, FDecodedSnapshot& Out)
	{
		Out = {};
		if (Data == nullptr || Size < sizeof(FSnapshotHeader))
		{
			return false;
		}

		FSnapshotHeader Header{};
		std::memcpy(&Header, Data, sizeof(Header));
		if (Header.Type != static_cast<std::uint8_t>(ENetMsg::Snapshot))
		{
			return false;
		}
		if (Header.PawnCount > MaxSnapshotPawns || Header.BodyCount > MaxDynamicBodies)
		{
			return false;
		}

		const std::size_t Needed = sizeof(Header) + Header.ExtBytes + (sizeof(FPawnSnap) * Header.PawnCount) +
			(sizeof(FBodySnap) * Header.BodyCount);
		if (Size < Needed)
		{
			return false;
		}

		Out.Tick = Header.Tick;
		std::size_t Offset = sizeof(Header);
		if (Header.ExtBytes == sizeof(FSnapshotMatchMeta))
		{
			std::memcpy(&Out.MatchMeta, Data + Offset, sizeof(FSnapshotMatchMeta));
			Out.bHasMatchMeta = true;
			Offset += sizeof(FSnapshotMatchMeta);
		}
		else if (Header.ExtBytes > 0)
		{
			// Unknown extension: skip so newer packs can add blobs without breaking older readers.
			Offset += Header.ExtBytes;
		}

		Out.Pawns.resize(Header.PawnCount);
		Out.Bodies.resize(Header.BodyCount);
		if (Header.PawnCount > 0)
		{
			std::memcpy(Out.Pawns.data(), Data + Offset, sizeof(FPawnSnap) * Header.PawnCount);
			Offset += sizeof(FPawnSnap) * Header.PawnCount;
		}
		if (Header.BodyCount > 0)
		{
			std::memcpy(Out.Bodies.data(), Data + Offset, sizeof(FBodySnap) * Header.BodyCount);
		}
		return true;
	}

} // namespace Leon::Net
