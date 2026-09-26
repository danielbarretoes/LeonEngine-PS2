#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTypes.h"

class UTexture2D;

/**
 * The GS local memory the scene renderer's textures live in (Leon; UE's texture streaming is the counterpart): an
 * arena of 64-word blocks the platform hands over, filled in first-use order. A texture is uploaded as PSMCT32 the
 * first time a list samples it (the upload goes into that list, before the draw) and stays resident until the arena
 * is full, when the cache starts over and every texture of the next draws uploads again. Each texture is a power of
 * two between MinTextureSize and MaxTextureSize a side (resampled nearest when it is not: the PS2 cook will make them
 * so, Docs/PLANS/ps2-engine.md E3) and takes whole pages, as the GS lays them out.
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

	TMap<const void*, FEntry> Entries;
	uint32 ArenaFirst = 0;
	uint32 ArenaBlocks = 0;
	/** The next free block, relative to ArenaFirst. */
	uint32 NextBlock = 0;
	int32 NumUploads = 0;
};
