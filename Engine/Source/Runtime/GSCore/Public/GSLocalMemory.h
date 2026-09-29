#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GSTypes.h"

/**
 * The GS's 4 MB local memory with the GS's own layout (GS User's Manual, chapter 8): a buffer's pixels are grouped in
 * pages (8 KB), a page in 32 blocks (256 bytes) arranged by a table of each storage format, a block in 4 columns (64
 * bytes) and a column's pixels in the order of the format's figure. The emulator, the reference rasterizer and the
 * texture cache all address memory through it, so the Win64 preview stores and reads what the GS does: two buffers
 * overlap here exactly when they overlap on the console.
 *
 * - A buffer is its base pointer in blocks of 64 words (TBP0, CBP, DBP; FBP and ZBP are pages, 32 blocks) and its width
 *   in units of 64 pixels (TBW, FBW, DBW). Pages follow each other left to right, BW x 64 / page width to a row (an
 *   8-bit or 4-bit buffer's BW is even on the GS; an odd one rounds down, as the GS's page arithmetic does).
 * - A pixel's block is the base pointer plus its page's first block plus the table's block: a base pointer inside a
 *   page shifts the table and runs into the next page (manual 8.5). Addresses wrap at 4 MB.
 * - PSMCT24, PSMZ24 and the PSMT8H / PSMT4HL / PSMT4HH formats share PSMCT32's (or PSMZ32's) words: a 24-bit pixel
 *   takes bits 0-23, PSMT8H bits 24-31, PSMT4HL 24-27 and PSMT4HH 28-31.
 */
class GSCORE_API FGSLocalMemory
{
public:
	static constexpr uint32 SizeInBytes = 4 * 1024 * 1024;
	static constexpr uint32 BytesPerPage = 8192;
	static constexpr uint32 BytesPerBlock = 256;
	static constexpr uint32 BytesPerColumn = 64;
	static constexpr uint32 BlocksPerPage = BytesPerPage / BytesPerBlock;
	static constexpr uint32 WordsPerBlock = BytesPerBlock / 4;
	static constexpr uint32 NumBlocks = SizeInBytes / BytesPerBlock;

	FGSLocalMemory();

	/**
	 * A pixel of a buffer at BasePointer (blocks) BufferWidth x 64 pixels wide, (X, Y) taken modulo 2048: 32 bits for
	 * the 32-bit and 24-bit formats (a 24-bit pixel keeps its word's high byte), 16, 8 or 4 for the others.
	 */
	[[nodiscard]] uint32 ReadPixel(
		uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y) const;
	/**
	 * Writes a pixel's bits as ReadPixel returns them (a 24-bit pixel's whole word: the caller keeps its high byte),
	 * leaving the rest of its word alone.
	 */
	void WritePixel(uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y, uint32 Value);

	/**
	 * A host to local transfer (manual 4.3): TrxReg's RRW x RRH pixels of BitBltBuf's destination format from TrxPos
	 * (wrapping at 2048), packed in Data as the transfer format (24-bit pixels in 3 bytes keeping the pixel's high
	 * byte, the first 4-bit pixel in the low nibble).
	 */
	void Transfer(
		const FGSBitBltBuf& BitBltBuf, const FGSTrxPos& TrxPos, const FGSTrxReg& TrxReg, TArrayView<const uint8> Data);

	/** The bits of a pixel of Format that ReadPixel returns (a 24-bit pixel returns its whole word). */
	[[nodiscard]] static uint32 StorageBits(EGSPixelFormat Format);

	/**
	 * A page of Format in pixels (manual 8.1): 64 x 32 for the 32-bit and 24-bit formats (PSMT8H, PSMT4HL and PSMT4HH
	 * too), 64 x 64 for 16-bit, 128 x 64 for PSMT8, 128 x 128 for PSMT4.
	 */
	static void GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight);
	/** A block of Format in pixels: 8 x 8, 16 x 8, 16 x 16 or 32 x 16 (a column is a quarter of its height). */
	static void GetBlockSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight);

	/** The block (0..31) holding pixel (X, Y) of a page of Format: the manual's block arrangement tables (8.3). */
	[[nodiscard]] static uint32 GetBlockInPage(EGSPixelFormat Format, uint32 X, uint32 Y);
	/** The block address (0..16383) of pixel (X, Y) of a buffer. */
	[[nodiscard]] static uint32 GetBlockAddress(
		uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y);
	/**
	 * The address of pixel (X, Y)'s lowest bit, in bits from the start of memory: its word is the result / 32, its
	 * bits start at the result % 32.
	 */
	[[nodiscard]] static uint32 GetBitAddress(
		uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y);

	/**
	 * The blocks from a buffer's base pointer that its Width x Height pixels from (0, 0) reach: the last block they
	 * touch plus one. A 16 x 16 PSMCT32 CLUT takes 4 blocks, an 8 x 2 one 1.
	 */
	[[nodiscard]] static uint32 GetBlockSpan(uint32 BufferWidth, EGSPixelFormat Format, uint32 Width, uint32 Height);

private:
	TArray<uint32> Words;
};
