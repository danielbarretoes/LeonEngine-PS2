#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"

/** A GIFtag's data format (FLG, EE User's Manual 7.2). */
enum class EGSGifFormat : uint8
{
	Packed = 0,
	RegList = 1,
	Image = 2,
};

/**
 * An FGSCommandList as the GIF receives it through PATH3 (EE User's Manual, chapter 7): runs of register writes in
 * PACKED mode with the A+D descriptor (one quadword per write: the value, then the register), and each HWREG transfer
 * as IMAGE mode quadwords. A GIFtag loops at most 0x7fff times, so long runs take several tags; the last tag has EOP.
 *
 * It is plain data on every platform, so the packing is tested on the host; the PS2 RHI sends it by DMA.
 */
struct GSCORE_API FGSGifPacket
{
	/** The most quadwords one GIFtag's NLOOP describes. */
	static constexpr uint32 MaxLoops = 0x7fff;
	/** The A+D register descriptor (REGS). */
	static constexpr uint64 AddressData = 0xe;

	/** A GIFtag's low 64 bits: NLOOP, EOP, FLG and NREG (PRE off). */
	[[nodiscard]] static uint64 MakeTag(uint32 Loops, bool bEndOfPacket, EGSGifFormat Format, uint32 NumRegisters);

	/**
	 * Appends List's packet to OutQuadwords as pairs of 64-bit words (low, high). With bFinish, a last FINISH write
	 * lets the sender wait for CSR.FINISH. Nothing is appended for an empty list without bFinish.
	 */
	static void Build(const FGSCommandList& List, bool bFinish, TArray<uint64>& OutQuadwords);
};
