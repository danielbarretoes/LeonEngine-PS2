#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTypes.h"
#include "PixelFormat.h"

class UTexture2D;

/**
 * The GS local memory the scene renderer's textures live in (Leon; UE's texture streaming is the counterpart): an
 * arena of 64-word blocks the platform hands over. A texture is uploaded the first time a list samples it (the upload
 * goes into that list, before the draw) and stays resident until the arena is full, when the cache starts over and
 * every texture of the next draws uploads again. Textures fill the arena from its start in whole pages
 * (FGSTextureLayout), CLUTs from its end in blocks.
 *
 * - PF_P8 / PF_P4 textures (the PS2 cook's, Docs/PLANS/ps2-engine.md E3) upload as they are: PSMT8 / PSMT4 indices
 *   and a PSMCT32 CLUT in CSM1, which TEX0 loads (CLD 1). Their sides must be powers of two between MinTextureSize and
 *   MaxTextureSize, as the cook makes them.
 * - RGBA8 textures (the desktop's content, the renderer's own texels) upload as PSMCT32, resampled nearest to powers of
 *   two in that range.
 *
 * Texels keep the engine's order (bottom row first, as OpenGL reads them), so a UV samples the same texel as on the
 * desktop renderer.
 */
class FGSTextureCache
{
public:
	static constexpr int32 MinTextureSize = 8;
	static constexpr int32 MaxTextureSize = 256;

	/** The arena: NumBlocks 64-word blocks from FirstBlock (page aligned). Empties the cache. */
	void SetArena(uint32 FirstBlock, uint32 NumBlocks);
	[[nodiscard]] bool HasArena() const
	{
		return ArenaBlocks > 0;
	}

	/**
	 * TEX0 for Texture (RGB from the texels, their alpha ignored, MODULATE), appending its upload to List first when it
	 * is not resident. False when it cannot be sampled: no arena, no valid texels.
	 */
	bool BindTexture(const UTexture2D& Texture, FGSCommandList& List, FGSTex0& OutTex0);

	/**
	 * TEX0 for texels the renderer makes (Key identifies them): Width x Height RGBA8, bottom row first, with their
	 * alpha (0..255 read as 0..0x80, 1.0 for the GS) when bAlpha.
	 */
	bool BindTexels(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
		FGSCommandList& List, FGSTex0& OutTex0);

	/** Forgets an asset's texture (its data changed or it is going away); its blocks come back when the arena resets.
	 */
	void Release(const void* Key);
	/** Forgets every texture. */
	void Reset();

	[[nodiscard]] int32 GetNumResident() const
	{
		return Entries.Num();
	}
	/** Uploads since the last ResetStats (a texture uploaded again after a reset counts again). */
	[[nodiscard]] int32 GetNumUploads() const
	{
		return NumUploads;
	}
	void ResetStats()
	{
		NumUploads = 0;
	}

private:
	struct FEntry
	{
		FGSTex0 Tex0;
	};

	/** Resamples and uploads; OutTex0 gets the buffer, size and format. */
	bool Upload(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
		FGSCommandList& List, FGSTex0& OutTex0);
	/** Uploads a paletted texture (the palette, then the indices) and its CLUT. */
	bool UploadPaletted(const void* Key, int32 SizeX, int32 SizeY, EPixelFormat Format, const uint8* Data,
		FGSCommandList& List, FGSTex0& OutTex0);
	/**
	 * Takes NumBlocks for texels and NumClutBlocks for a CLUT (block numbers in OutBlock, OutClutBlock), starting over
	 * when they do not fit; false when they never can.
	 */
	bool Allocate(uint32 NumBlocks, uint32 NumClutBlocks, uint32& OutBlock, uint32& OutClutBlock);

	TMap<const void*, FEntry> Entries;
	uint32 ArenaFirst = 0;
	uint32 ArenaBlocks = 0;
	/** The next free block for texels, relative to ArenaFirst. */
	uint32 NextBlock = 0;
	/** The blocks the CLUTs take at the arena's end. */
	uint32 ClutBlocks = 0;
	int32 NumUploads = 0;
};
