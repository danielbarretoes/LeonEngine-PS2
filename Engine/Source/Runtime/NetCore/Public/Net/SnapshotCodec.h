#pragma once

#include "Net/NetProtocol.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Leon::Net
{

	/// Decoded authoritative snapshot (client interpolation targets).
	struct NETCORE_API FDecodedSnapshot
	{
		std::uint32_t Tick = 0;
		bool bHasMatchMeta = false;
		FSnapshotMatchMeta MatchMeta{};
		std::vector<FPawnSnap> Pawns;
		std::vector<FBodySnap> Bodies;

		/// Convenience mirrors when hasMatchMeta (else 0).
		[[nodiscard]] std::uint8_t RoundIndex() const
		{
			return bHasMatchMeta ? MatchMeta.RoundIndex : 0;
		}
		[[nodiscard]] std::uint8_t UnitsAlive() const
		{
			return bHasMatchMeta ? MatchMeta.UnitsAlive : 0;
		}
		[[nodiscard]] std::uint8_t RemainingSeconds() const
		{
			return bHasMatchMeta ? MatchMeta.RemainingSeconds : 0;
		}
	};

	/// Pack FSnapshotHeader [| MatchMeta] | pawns | bodies.
	/// When matchMeta is non-null, writes sizeof(FSnapshotMatchMeta) extension after the header.
	[[nodiscard]] bool EncodeSnapshot(std::vector<std::uint8_t>& OutPacket, std::uint32_t InTick,
		const FPawnSnap* InPawns, std::uint8_t PawnCount, const FBodySnap* InBodies, std::uint8_t BodyCount,
		const FSnapshotMatchMeta* InMatchMeta = nullptr);

	/// Parse a Snapshot datagram (validates sizes; unknown extBytes are skipped).
	[[nodiscard]] bool DecodeSnapshot(const std::uint8_t* Data, std::size_t Size, FDecodedSnapshot& Out);

} // namespace Leon::Net
