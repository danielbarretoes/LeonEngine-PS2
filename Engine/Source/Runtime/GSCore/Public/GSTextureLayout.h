#pragma once

#include "CoreMinimal.h"
#include "GSTypes.h"

/**
 * Where a texture goes in GS local memory (GS User's Manual, chapter 8): its pages, its buffer width and its CLUT.
 * The scene renderer's texture cache lays textures out with it, and the PS2 cook's VRAM report counts with it, so both
 * agree on what a texture costs.
 *
 * A texture takes whole pages of its format (8 KB, 32 blocks of 64 words), so that each one starts on a page and the
 * pages' block order holds; its buffer width (TBW, in units of 64 texels) spans whole pages across, which makes it even
 * for PSMT8 and PSMT4 as the GS asks. An indexed texture's CLUT is PSMCT32 in CSM1: a 16 x 16 image for PSMT8, 8 x 2
 * for PSMT4, at a block address of its own.
 */
struct GSCORE_API FGSTextureLayout
{
	static constexpr uint32 BlocksPerPage = 32;
	static constexpr uint32 BytesPerBlock = 256;

	/** A page of Format in texels: 64 x 32 (32 and 24 bits), 64 x 64 (16), 128 x 64 (8), 128 x 128 (4). */
	static void GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight);

	/** TBW for a texture Width texels wide: the pages across, in units of 64 texels. */
	[[nodiscard]] static uint8 GetBufferWidth(EGSPixelFormat Format, uint32 Width);

	/** The blocks of the whole pages a Width x Height texture of Format takes. */
	[[nodiscard]] static uint32 GetNumBlocks(EGSPixelFormat Format, uint32 Width, uint32 Height);

	/** The blocks of an indexed format's PSMCT32 CLUT: 4 for PSMT8, 1 for PSMT4, 0 for the others. */
	[[nodiscard]] static uint32 GetClutBlocks(EGSPixelFormat Format);

	/**
	 * The CLUT image of a palette (Palette[i] = RGBA8 entry i) as the GS reads it in CSM1: 16 x 16 for 256 entries,
	 * entries 8..15 and 16..23 of each 32 trading places; 8 x 2 for 16 entries. Each texel is a PSMCT32 word (R in the
	 * low byte), its alpha scaled from 0..255 to the GS's 0..0x80.
	 */
	static void MakeClutImage(
		TArrayView<const uint32> Palette, TArray<uint8>& OutImage, uint16& OutWidth, uint16& OutHeight);
};
