#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"

/**
 * A texture's paletted platform data (PF_P8 or PF_P4) with its mip chain, load-in-place for the GS
 * (Docs/PLANS/ps2-shipping.md N23): the CLUT as the GS reads it, then each mip's indices as the GIF uploads them,
 * bottom row first.
 */
struct FPalettedTexture
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	EPixelFormat Format = PF_Unknown;
	/**
	 * Mip 0: GetPixelFormatDataSize(Format, SizeX, SizeY) bytes. First the CLUT image (FGSTextureLayout::MakeClutImage:
	 * PSMCT32 in CSM1 order, 16 x 16 or 8 x 2 texels, each entry's alpha scaled to the GS's 0..0x80), then the indices
	 * (PSMT4: the first texel in the low nibble). Both are whole quadwords, so the texture cache uploads each as it is.
	 */
	TArray<uint8> Data;
	/** The palette's RGBA8 entries (entry i of the indices), before the CLUT's reordering and alpha scaling. */
	TArray<uint32> Palette;
	/**
	 * Mips 1 and on, each half the one before on both sides: their indices only (GetPixelFormatMipDataSize), into
	 * Data's palette.
	 */
	TArray<TArray<uint8>> Mips;
	/** The distinct colours of mip 0; more than 256 means the palette approximates them. */
	int32 NumSourceColors = 0;
};

/**
 * The PS2 cook's texture conversion (Docs/PLANS/ps2-engine.md E3, UE's texture format modules): RGBA8 texels to the
 * GS's indexed formats with their MIPMAP levels, byte for byte the same on every run.
 *
 * - The sides become powers of two between MinSize and MaxSize (the nearest one, each texel the average of the source
 *   texels it covers; the GS samples powers of two only).
 * - The mip chain (Docs/PLANS/ps2-shipping.md N13) halves both sides down to MinSize on the shorter one (GetNumMips):
 *   each texel of a mip is the box average of the four under it, in linear space for an sRGB texture, its colour
 *   weighted by their alpha (a cut-out's transparent texels do not darken its edge).
 * - One palette for every mip, since the GS reads a texture's levels through one CLUT. Mip 0 decides the format: up to
 *   16 distinct colours give PF_P4, up to 256 PF_P8, and mip 0 stays exact; the palette's free entries take the mips'
 *   other colours by median cut, and a mip texel takes the nearest entry. More than 256 colours in mip 0 are reduced
 *   together with the mips' by median cut: the box with the widest channel splits at its pixel median along that
 *   channel, ties broken by the box's order and the colours' values, and each box's colour is its pixels' mean.
 */
class TEXTURECOMPRESSOR_API FPalettedTextureBuilder
{
public:
	static constexpr int32 MinSize = 8;
	static constexpr int32 MaxSize = 256;
	/** Level 0 and the GS's six MIPMAP levels (TEX1.MXL at most 6). */
	static constexpr int32 MaxMips = 7;

	/** The power of two between MinSize and MaxSize nearest Size (the larger on a tie). */
	[[nodiscard]] static int32 GetPowerOfTwoSize(int32 Size);

	/** The mips of a SizeX x SizeY texture (powers of two): halved until the shorter side is MinSize. */
	[[nodiscard]] static int32 GetNumMips(int32 SizeX, int32 SizeY);

	/**
	 * Builds Out from Width x Height RGBA8 texels (bottom row first), its mips averaged in linear space when bSRGB;
	 * false for an empty image.
	 */
	static bool Build(const uint8* Rgba, int32 Width, int32 Height, bool bSRGB, FPalettedTexture& Out);
};
