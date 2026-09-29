#pragma once

#include "CoreMinimal.h"
#include "GSLocalMemory.h"
#include "GSTypes.h"

/** The CLUT temporary buffer: 256 32-bit or 512 16-bit entries (manual 3.4.7), and its load control. */
struct GSCORE_API FGSClutBuffer
{
	uint32 Entries[512] = {};
	/** The internal registers CBP0 and CBP1 that CLD 2 to 5 set and compare (manual 3.4.7). */
	uint16 CBP0 = 0;
	uint16 CBP1 = 0;

	/**
	 * What a TEX0 write does to the buffer by its CLD (manual 3.4.7): 0 keeps it, 1 loads, 2 / 3 load and set CBP0 /
	 * CBP1 to CBP, 4 / 5 load only when CBP0 / CBP1 differs from CBP and then set it (the pair is a cache of which CLUT
	 * the buffer holds: the manual does not say that 4 / 5 set the register, but a cache that never learns would load
	 * every time); 6 and 7 are reserved and keep it. Only a CLUT format (PSMT8, PSMT4) looks at CLD at all. Returns
	 * whether it loaded.
	 */
	bool Update(const FGSLocalMemory& Memory, const FGSTex0& Tex0);

	/**
	 * Loads Tex0's CLUT from the local memory: a CSM1 image of CPSM at CBP (a buffer 64 pixels wide, read
	 * with the GS's layout), IDTEX8's 256 entries in its 16 x 16 rectangle with bits 3 and 4 of the index swapped,
	 * IDTEX4's 16 in its 8 x 2 one, into the temporary buffer at CSA * 16. CSM2 is not in the contract
	 * (FGSCommandList rejects it).
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
	 * shifted right by Level: REGION_CLAMP's range, REGION_REPEAT's masks and fixed bits.
	 */
	[[nodiscard]] static int32 Wrap(
		EGSWrapMode Mode, int32 Coordinate, int32 Size, uint32 Level, uint16 Min, uint16 Max);

	/**
	 * The RGBA of the texel at (U, V), already wrapped, of a level of Tex0's format stored at BasePointer (64-word
	 * blocks) in a buffer BufferWidth x 64 pixels wide (FGSLocalMemory's layout); a CLUT format goes through Clut.
	 */
	[[nodiscard]] static FColor Decode(const FGSLocalMemory& Memory, const FGSTex0& Tex0, uint32 BasePointer,
		uint32 BufferWidth, const FGSTexA& TexA, const FGSClutBuffer& Clut, uint32 U, uint32 V);
};
