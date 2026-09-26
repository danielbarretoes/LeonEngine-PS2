#pragma once

#include "CoreMinimal.h"
#include "GSLocalMemory.h"
#include "GSTypes.h"

/** The CLUT temporary buffer: 256 32-bit or 512 16-bit entries (manual 3.4.7). */
struct GSCORE_API FGSClutBuffer
{
	uint32 Entries[512] = {};

	/**
	 * Loads Tex0's CLUT (CLD = 1) from the local memory at CBP, a buffer 64 pixels wide: IDTEX8's 256 entries in the
	 * CSM1 layout (bits 3 and 4 of the index swapped in its 16 x 16 rectangle), IDTEX4's 16 at CSA * 16.
	 */
	void Load(const FGSLocalMemory& Memory, const FGSTex0& Tex0);
};

/**
 * Texels as the GS reads them (manual 3.4), shared by the reference rasterizer and the desktop's GS emulator so both
 * decode the same colours: the wrap modes, the formats (5-bit colours shifted left 3, TEXA's alpha) and the CLUTs.
 */
struct GSCORE_API FGSTexelDecoder
{
	/** A 16-bit or 24-bit colour with TEXA's alpha (a 24-bit one takes TA0; black takes 0 with AEM). */
	[[nodiscard]] static FColor ExpandColor(uint32 Value, EGSPixelFormat Format, const FGSTexA& TexA);

	/**
	 * One texel coordinate of a level of Size texels by the wrap mode; the region modes take MINU / MAXU (MINV / MAXV)
	 * shifted right by Level for REGION_CLAMP, as masks and fixed bits for REGION_REPEAT.
	 */
	[[nodiscard]] static int32 Wrap(
		EGSWrapMode Mode, int32 Coordinate, int32 Size, uint32 Level, uint16 Min, uint16 Max);

	/**
	 * The RGBA of the texel at (U, V), already wrapped, of a level of Tex0's format stored at BasePointer (64-word
	 * blocks) BufferWidth x 64 pixels wide; a CLUT format goes through Clut.
	 */
	[[nodiscard]] static FColor Decode(const FGSLocalMemory& Memory, const FGSTex0& Tex0, uint32 BasePointer,
		uint32 BufferWidth, const FGSTexA& TexA, const FGSClutBuffer& Clut, uint32 U, uint32 V);
};
