#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"

/** A texture's paletted platform data (PF_P8 or PF_P4): the palette, then the indices, bottom row first. */
struct FPalettedTexture
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	EPixelFormat Format = PF_Unknown;
	/** GetPixelFormatDataSize(Format, SizeX, SizeY) bytes: the RGBA8 palette entries, then the indices. */
	TArray<uint8> Data;
	/** The distinct colours of the resized texels; more than 256 means the palette approximates them. */
	int32 NumSourceColors = 0;
};

/**
 * The PS2 cook's texture conversion (Docs/PLANS/ps2-engine.md E3, UE's texture format modules): RGBA8 texels to the
 * GS's indexed formats, byte for byte the same on every run.
 *
 * - The sides become powers of two between MinSize and MaxSize (the nearest one, each texel the average of the source
 *   texels it covers; the GS samples powers of two only).
 * - Up to 16 distinct colours give PF_P4, up to 256 PF_P8, both exact. More are reduced to 256 by median cut: the box
 *   with the widest channel splits at its pixel median along that channel, ties broken by the box's order and the
 *   colours' values, and each box's colour is its pixels' mean.
 */
class TEXTURECOMPRESSOR_API FPalettedTextureBuilder
{
public:
	static constexpr int32 MinSize = 8;
	static constexpr int32 MaxSize = 256;

	/** The power of two between MinSize and MaxSize nearest Size (the larger on a tie). */
	[[nodiscard]] static int32 GetPowerOfTwoSize(int32 Size);

	/** Builds Out from Width x Height RGBA8 texels (bottom row first); false for an empty image. */
	static bool Build(const uint8* Rgba, int32 Width, int32 Height, FPalettedTexture& Out);
};
