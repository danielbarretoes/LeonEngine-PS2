#pragma once

#include "CoreMinimal.h"
#include "GS/GSTextureCache.h"

/**
 * The desktop's FGSTextureCache converter (Docs/PLANS/ps2-preview.md V1): an uncooked asset's RGBA8 texels as the PS2
 * cook makes them (FPalettedTextureBuilder: powers of two up to 256, PSMT4 / PSMT8, the mip chain through one palette),
 * so the desktop draws the colours, sizes and levels the console does; UE's editor derives a texture's data for its
 * preview platform the same way.
 */
bool ConvertTextureAsPS2Cook(
	const uint8* Rgba, int32 Width, int32 Height, bool bSRGB, FGSTextureCache::FConvertedTexture& Out);
