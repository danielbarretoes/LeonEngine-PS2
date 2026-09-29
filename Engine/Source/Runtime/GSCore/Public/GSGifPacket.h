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

/** A source chain DMAtag's ID (EE User's Manual 5.6). */
enum class EGSDmaTag : uint8
{
	/** The QWC quadwords at ADDR, then the chain ends. */
	RefE = 0,
	/** The QWC quadwords after the tag; the next tag follows them. */
	Cnt = 1,
	/** The QWC quadwords after the tag; the next tag is at ADDR. */
	Next = 2,
	/** The QWC quadwords at ADDR; the next tag follows this one. */
	Ref = 3,
	/** REF with the stall control of the destination channel. */
	RefS = 4,
	/** The QWC quadwords after the tag, then the chain at ADDR, returning after the data. */
	Call = 5,
	/** The QWC quadwords after the tag, then back to where CALL left. */
	Ret = 6,
	/** The QWC quadwords after the tag, then the chain ends. */
	End = 7,
};

/** A VIFcode's CMD (EE User's Manual 6.3.2; UNPACK's low bits are its format). */
enum class EGSVifCommand : uint8
{
	Nop = 0x00,
	StCycl = 0x01,
	Offset = 0x02,
	Base = 0x03,
	Itop = 0x04,
	StMod = 0x05,
	MskPath3 = 0x06,
	Mark = 0x07,
	/** Waits for the end of the microprogram. */
	FlushE = 0x10,
	/** Waits for the end of the microprogram and of the GIF's PATH1 and PATH2 transfers. */
	Flush = 0x11,
	/** FLUSH, and no PATH3 request either. */
	FlushA = 0x13,
	/** Starts the microprogram at IMMEDIATE (in instructions) once the running one ends. */
	MsCal = 0x14,
	MsCalF = 0x15,
	MsCnt = 0x17,
	/** Loads NUM instructions at IMMEDIATE of the micro memory; the code must start on 64 bits. */
	Mpg = 0x4a,
	/** Sends the IMMEDIATE quadwords that follow (from a quadword) to the GIF by PATH2. */
	Direct = 0x50,
	DirectHL = 0x51,
	/** UNPACK S-32; ORed with its format: V2 0x04, V3 0x08, V4 0x0c, and 16 bits 0x01, 8 bits 0x02, 5 bits 0x03. */
	Unpack = 0x60,
};

struct FGSVertexBatch;

/**
 * Writes an FGSCommandList's vertex batches into its DMA chain (FGSGifPacket::BuildChain): the platform's VU1
 * microprograms, their data layout and their constants (the PS2 RHI's). It writes whole DMAtags with their VIFcodes.
 */
class IGSVertexBatchEncoder
{
public:
	virtual ~IGSVertexBatchEncoder() = default;

	/** The most quadwords WritePrologue writes. */
	[[nodiscard]] virtual uint32 GetPrologueQuadwords() const = 0;
	/** The most quadwords WriteBatch writes for one batch. */
	[[nodiscard]] virtual uint32 GetMaxBatchQuadwords() const = 0;
	/** Writes what the batches need before the first (VIF1's registers, VU1's shared constants); returns the end. */
	virtual uint64* WritePrologue(uint64* Out) = 0;
	/** Writes List's batch: its data to VU1's memory and the start of its microprogram; returns the end. */
	virtual uint64* WriteBatch(const FGSCommandList& List, const FGSVertexBatch& Batch, uint64* Out) = 0;
};

/**
 * An FGSCommandList as the GIF receives it (EE User's Manual, chapter 7): runs of register writes in PACKED mode with
 * the A+D descriptor (one quadword per write: the value, then the register), and each HWREG transfer as IMAGE mode
 * quadwords. A GIFtag loops at most 0x7fff times, so long runs take several tags; the last tag has EOP.
 *
 * The same packet comes two ways, from one encoder: Build appends it to an array (what the tests read), and BuildChain
 * writes it as the DMA source chain the PS2 RHI sends to VIF1 (chapters 5 and 6; Docs/PLANS/ps2-shipping.md N14): the
 * tags and register writes in CNT / END sections whose DMAtags carry a VIFcode DIRECT (PATH2), each upload's pixels by
 * a REF tag to the command list's own image data, which the DMA reads in place, and the vertex batches as an
 * IGSVertexBatchEncoder writes them (VU1's packets, PATH1). It is plain data on every platform, so the packing is
 * tested on the host (and on the EE by TestPAL).
 */
struct GSCORE_API FGSGifPacket
{
	/** The most quadwords one GIFtag's NLOOP describes. */
	static constexpr uint32 MaxLoops = 0x7fff;
	/** The A+D register descriptor (REGS). */
	static constexpr uint64 AddressData = 0xe;
	/** The most quadwords one DMAtag's QWC moves (and one DIRECT's IMMEDIATE). */
	static constexpr uint32 MaxDmaQuadwords = 0xffff;
	/** EOP, bit 15 of a GIFtag. */
	static constexpr uint64 EndOfPacketBit = uint64(1) << 15;

	/** A GIFtag's low 64 bits: NLOOP, EOP, FLG and NREG (PRE off). */
	[[nodiscard]] static uint64 MakeTag(uint32 Loops, bool bEndOfPacket, EGSGifFormat Format, uint32 NumRegisters);

	/**
	 * Appends List's packet to OutQuadwords as pairs of 64-bit words (low, high). With bFinish, a last FINISH write
	 * lets the sender wait for CSR.FINISH. Nothing is appended for an empty list without bFinish. List must have no
	 * vertex batch (VU1 makes their packets: BuildChain).
	 */
	static void Build(const FGSCommandList& List, bool bFinish, TArray<uint64>& OutQuadwords);

	/** A DMAtag's low 64 bits: QWC bits 0-15, ID bits 28-30, ADDR bits 32-62 (PCE, IRQ and SPR off). */
	[[nodiscard]] static uint64 MakeDmaTag(uint32 Quadwords, EGSDmaTag Id, uint32 Address = 0);
	/** A VIFcode: IMMEDIATE bits 0-15, NUM bits 16-23, CMD bits 24-31 (no interrupt). */
	[[nodiscard]] static uint32 MakeVifCode(EGSVifCommand Command, uint32 Num = 0, uint32 Immediate = 0);
	/** A DMAtag's high 64 bits with the tag transfer on: the two VIFcodes VIF1 runs before the tag's data. */
	[[nodiscard]] static uint64 MakeVifCodes(uint32 First, uint32 Second)
	{
		return uint64(First) | (uint64(Second) << 32);
	}

	/**
	 * The ADDR a DMAtag takes for Data in the EE's main memory: its physical address, the segment bits (cached,
	 * uncached, uncached accelerated) cleared. On the host it only identifies the data.
	 */
	[[nodiscard]] static uint32 GetDmaAddress(const void* Data);

	/** The most quadwords BuildChain writes for List (the buffer it needs), with Encoder's for its vertex batches. */
	[[nodiscard]] static uint32 GetChainCapacity(
		const FGSCommandList& List, bool bFinish, const IGSVertexBatchEncoder* Encoder = nullptr);

	/**
	 * Writes List's packet (Build's, with bFinish the same way) as a DMA source chain for VIF1 into OutQuadwords, which
	 * holds CapacityQuadwords (at least GetChainCapacity) and returns the quadwords written. It is sent with the tag
	 * transfer on (TTE): each DMAtag's high 64 bits are two VIFcodes.
	 *
	 * - The GIFtags and register writes go in CNT sections of at most MaxDmaQuadwords, whose VIFcodes are a NOP and a
	 *   DIRECT of the section (PATH2); each upload's pixels in a REF to List's image data (quadword aligned: the DMA
	 *   reads it where it is, so List must outlive the transfer) with a DIRECT of its own; an END section closes the
	 *   chain (an empty list without bFinish is an END of no quadwords, its VIFcodes two NOPs).
	 * - The vertex batches go as Encoder writes them (a list with batches needs one), after its prologue. The GS
	 *   writes before a batch end their GIF packet (EOP) so that VU1's packet may take the GIF (PATH1), and the first
	 *   section after batches starts with a FLUSH (the microprograms ended and their packets sent) so that the writes
	 *   after a batch reach the GS after it. That is the whole ordering: nothing goes by PATH3.
	 *
	 * OutQuadwords is only written, never read back: on the PS2 it is the uncached accelerated segment's address of the
	 * buffer.
	 */
	static uint32 BuildChain(const FGSCommandList& List, bool bFinish, uint64* OutQuadwords, uint32 CapacityQuadwords,
		IGSVertexBatchEncoder* Encoder = nullptr);
};
