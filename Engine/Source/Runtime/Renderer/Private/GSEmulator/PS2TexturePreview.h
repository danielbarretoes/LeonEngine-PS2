#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"

/**
 * The desktop's FGSTextureCache converter (Docs/PLANS/ps2-preview.md V1): an uncooked asset's RGBA8 texels as the PS2
 * cook makes them (FPalettedTextureBuilder: powers of two up to 256, PSMT4 / PSMT8), so the desktop draws the colours
 * and sizes the console does; UE's editor derives a texture's data for its preview platform the same way.
 */
bool ConvertTextureAsPS2Cook(const uint8* Rgba, int32 Width, int32 Height, int32& OutSizeX, int32& OutSizeY,
	EPixelFormat& OutFormat, TArray<uint8>& OutData);
