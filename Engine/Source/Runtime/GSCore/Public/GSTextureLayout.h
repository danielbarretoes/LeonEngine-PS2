#pragma once

#include "CoreMinimal.h"
#include "GSLocalMemory.h"
#include "GSTypes.h"

/**
 * Where a texture goes in GS local memory (GS User's Manual, chapter 8): its blocks, its buffer width and its CLUT.
 * The scene renderer's texture cache lays textures out with it, and the PS2 cook's VRAM report counts with it, so both
 * agree on what a texture costs. The footprints come from FGSLocalMemory's layout, the GS's own.
 *
 * A texture's buffer width (TBW, in units of 64 texels) spans whole pages across, which makes it even for PSMT8 and
 * PSMT4 as the GS asks. A texture that fits in a page takes the blocks its texels reach from its base pointer, which
 * may be any block (an 8 x 8 PSMCT32 takes 1, a 64 x 64 PSMT8 16); a larger one starts on a page and takes the blocks
 * up to the last one it reaches. An indexed texture's CLUT is PSMCT32 in CSM1: a 16 x 16 image for PSMT8 (4 blocks),
 * 8 x 2 for PSMT4 (1 block), at a block address of its own.
 */
struct GSCORE_API FGSTextureLayout
{
	static constexpr uint32 BlocksPerPage = FGSLocalMemory::BlocksPerPage;
	static constexpr uint32 BytesPerBlock = FGSLocalMemory::BytesPerBlock;
	/** Level 0 and the six MIPMAP levels MIPTBP1 / MIPTBP2 point at. */
	static constexpr int32 MaxLevels = 7;

	/**
	 * Where a texture's MIPMAP levels and its CLUT go from its base block (Docs/PLANS/ps2-shipping.md N13): level 0 at
	 * the base, each next level after the one before at its own alignment (a level that fits in a page from any block,
	 * a larger one from a page: the base is aligned as level 0 asks, so every offset keeps its alignment), then an
	 * indexed format's CLUT. One run of blocks, which the texture cache allocates and the cook's VRAM report counts.
	 */
	struct FFootprint
	{
		int32 NumLevels = 1;
		/** Each level's first block from the base and its TBW. */
		uint32 LevelBlock[MaxLevels] = {};
		uint8 LevelBufferWidth[MaxLevels] = {};
		/** The CLUT's first block from the base (GetClutBlocks of them; 0 for a format without one). */
		uint32 ClutBlock = 0;
		/** The blocks of the run: the levels and the CLUT. */
		uint32 NumBlocks = 0;
		/** What the base block must be a multiple of: level 0's GetBaseAlignment. */
		uint32 Alignment = 1;
	};

	/** The footprint of NumLevels levels (1 to MaxLevels) of a Width x Height texture of Format, each half the last. */
	static void GetFootprint(EGSPixelFormat Format, uint32 Width, uint32 Height, int32 NumLevels, FFootprint& Out);

	/** A page of Format in texels: 64 x 32 (32 and 24 bits), 64 x 64 (16), 128 x 64 (8), 128 x 128 (4). */
	static void GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight);

	/** TBW for a texture Width texels wide: the pages across, in units of 64 texels. */
	[[nodiscard]] static uint8 GetBufferWidth(EGSPixelFormat Format, uint32 Width);

	/** The blocks a Width x Height texture of Format takes from its base pointer, at GetBufferWidth. */
	[[nodiscard]] static uint32 GetNumBlocks(EGSPixelFormat Format, uint32 Width, uint32 Height);

	/** The blocks a texture's base pointer is a multiple of: 1 when it fits in a page, a page's 32 otherwise. */
	[[nodiscard]] static uint32 GetBaseAlignment(EGSPixelFormat Format, uint32 Width, uint32 Height);

	/** The blocks of an indexed format's PSMCT32 CLUT: 4 for PSMT8, 1 for PSMT4, 0 for the others. */
	[[nodiscard]] static uint32 GetClutBlocks(EGSPixelFormat Format);

	/**
	 * The CLUT image of a palette (Palette[i] = RGBA8 entry i) as the GS reads it in CSM1: 16 x 16 for 256 entries,
	 * entries 8..15 and 16..23 of each 32 trading places; 8 x 2 for 16 entries. Each texel is a PSMCT32 word (R in the
	 * low byte), its alpha scaled from 0..255 to the GS's 0..0x80. Upload it with DBW = 1.
	 */
	static void MakeClutImage(
		TArrayView<const uint32> Palette, TArray<uint8>& OutImage, uint16& OutWidth, uint16& OutHeight);

	/** The texel of MakeClutImage's image that holds palette entry Index (CSM1: bits 3 and 4 swapped for PSMT8). */
	[[nodiscard]] static int32 GetClutImagePosition(int32 Index, bool bIndex8)
	{
		return bIndex8 ? ((Index & ~0x18) | ((Index & 0x08) << 1) | ((Index & 0x10) >> 1)) : Index;
	}

	/** The CLUT image's size in texels: 16 x 16 for PSMT8, 8 x 2 for PSMT4. */
	static void GetClutImageSize(bool bIndex8, uint16& OutWidth, uint16& OutHeight)
	{
		OutWidth = bIndex8 ? 16 : 8;
		OutHeight = bIndex8 ? 16 : 2;
	}
};
