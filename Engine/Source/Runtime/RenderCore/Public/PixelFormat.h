#pragma once

#include "CoreMinimal.h"

/**
 * The layout of a texture's texels (UE: EPixelFormat, Core's PixelFormat.h; Leon keeps it in RenderCore, next to the
 * other CPU render data). Only the formats Leon stores are listed, with UE's values, which the texture assets save; the
 * paletted formats are Leon's (the PS2 cook's), numbered past UE's.
 */
enum EPixelFormat : uint8
{
	PF_Unknown = 0,
	/** 8-bit blue, green, red, alpha (UE's default for transient textures). */
	PF_B8G8R8A8 = 2,
	/** 8-bit red, green, blue, alpha: Leon's texture assets as imported. */
	PF_R8G8B8A8 = 37,
	/**
	 * Leon: 8-bit indices into a palette of 256 RGBA8 colours (the GS's PSMT8 with a PSMCT32 CLUT). The data is the
	 * palette, then one index per texel.
	 */
	PF_P8 = 200,
	/**
	 * Leon: 4-bit indices into a palette of 16 RGBA8 colours (PSMT4). The data is the palette, then two texels a byte,
	 * the first in the low nibble.
	 */
	PF_P4 = 201,
};

/** Bytes of one texel of a direct colour format (UE: GPixelFormats[Format].BlockBytes); 0 for the others. */
[[nodiscard]] inline int32 GetPixelFormatBytes(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_B8G8R8A8:
		case PF_R8G8B8A8:
			return 4;
		default:
			return 0;
	}
}

/** The palette entries of a paletted format (256 for PF_P8, 16 for PF_P4); 0 for the others. */
[[nodiscard]] inline int32 GetPixelFormatPaletteSize(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_P8:
			return 256;
		case PF_P4:
			return 16;
		default:
			return 0;
	}
}

/**
 * Bytes of a SizeX x SizeY image of Format, its palette included (UE: CalcTextureMipMapSize); 0 for PF_Unknown, an
 * empty size, or 4-bit indices that do not fill whole bytes.
 */
[[nodiscard]] inline int64 GetPixelFormatDataSize(EPixelFormat Format, int32 SizeX, int32 SizeY)
{
	if (SizeX <= 0 || SizeY <= 0)
	{
		return 0;
	}
	const int64 NumTexels = int64(SizeX) * SizeY;
	switch (Format)
	{
		case PF_B8G8R8A8:
		case PF_R8G8B8A8:
			return NumTexels * 4;
		case PF_P8:
			return (256 * 4) + NumTexels;
		case PF_P4:
			return (NumTexels % 2) == 0 ? (16 * 4) + (NumTexels / 2) : 0;
		default:
			return 0;
	}
}
