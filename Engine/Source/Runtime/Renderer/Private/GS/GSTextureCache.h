#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSTextureLayout.h"
#include "GSTypes.h"
#include "PixelFormat.h"

class UTexture2D;

/** How a draw samples a texture the cache bound (FGSTextureCache::BindTexture). */
struct FGSTextureBinding
{
	/** TEX0: the texture's level 0 (or the one level resident), its CLUT and the CLUT's load control. */
	FGSTex0 Tex0;
	/** The base pointers and widths of MIPMAP levels 1 to 3 and 4 to 6 (the last level repeated past NumLevels). */
	FGSMipTbp MipTbp1;
	FGSMipTbp MipTbp2;
	/** The levels the draw may sample (1: level 0 only): TEX1.MXL is one less. */
	int32 NumLevels = 1;
	/**
	 * The texture is not resident this frame (the upload budget or a full arena): the draw stands in with a flat
	 * colour, FlatColor (the texels' average, RGBA8 as stored), and no texture.
	 */
	bool bFlat = false;
	FColor FlatColor = FColor(255, 255, 255, 255);
};

/**
 * The GS local memory the scene renderer's textures live in (Leon; UE's texture streaming is the counterpart): an
 * arena of 64-word blocks the platform hands over (Docs/PLANS/ps2-shipping.md N13).
 *
 * - Residency. A texture takes one run of blocks (FGSTextureLayout::FFootprint: its MIPMAP levels at their real
 *   alignment, one that fits in a page from any block, a larger one from a page, then its CLUT), the first run that
 *   fits from the arena's start. When none does, the least recently bound textures that the frame has not bound are
 *   evicted, oldest first, until one does; a texture the frame has bound is never evicted, and the arena is never
 *   emptied while a frame draws from it. If the textures of the frame alone fill it, the next one draws flat.
 * - The upload budget (`[/Script/Engine.RendererSettings] TextureUploadBudgetKB`). A frame uploads at most that many
 *   KB of texels and CLUTs (the first upload of a frame always goes, so no texture waits forever). A texture over the
 *   budget uploads its smallest level and its CLUT, if they fit, and draws with that level alone; otherwise it draws
 *   with a flat colour, its texels' average. The next frames upload the rest.
 * - CLUT loads (the GS's CLD, manual 3.4.7). The cache knows which CLUT the GS's temporary buffer holds: a PSMT8 CLUT
 *   fills it (CBP0, CSA 0), a PSMT4 CLUT takes 16 entries at CSA 0 (CBP0) or CSA 1 (CBP1). TEX0 asks for CLD 4 / 5, so
 *   the GS loads only when CBP0 / CBP1 differs; when a register names the CLUT but the buffer no longer holds it (a
 *   PSMT8 load overwrote the entries, or an upload rewrote the CLUT's blocks: two textures that took the same blocks
 *   one after the other), TEX0 asks for CLD 2 / 3, which loads anyway. Every TEX0 BindTexture returns must be written
 *   to the list, in order: the cache's picture of the buffer follows those writes.
 *
 * Formats: PF_P8 / PF_P4 textures (the PS2 cook's) upload as they are, PSMT8 / PSMT4 indices with a PSMCT32 CLUT in
 * CSM1, with their mips (every mip through the one CLUT). The cook stores them load-in-place
 * (Docs/PLANS/ps2-shipping.md N23): the CLUT image first, then each level's indices, each the exact IMAGE transfer
 * payload, so the cache neither converts nor copies them: the list holds them in place
 * (FGSCommandList::UploadImageInPlace) and the PS2's DMA chain reads them where the texture keeps them. Their sides
 * must be powers of two between MinTextureSize and MaxTextureSize. RGBA8 textures (the renderer's own texels, and the
 * assets of uncooked content) upload as PSMCT32, level 0 only, resampled nearest to powers of two in that range, unless
 * a texture converter is set: the desktop's turns an uncooked asset's texels into what the PS2 cook makes
 * (Docs/PLANS/ps2-preview.md V1), once, and uploads that.
 *
 * Texels keep the engine's order (bottom row first, as OpenGL reads them), so a UV samples the same texel as on the
 * desktop renderer.
 */
class FGSTextureCache
{
public:
	static constexpr int32 MinTextureSize = 8;
	static constexpr int32 MaxTextureSize = 256;

	/** An asset's texels as the PS2 cook makes them: level 0 with its palette, then the mips' indices. */
	struct FConvertedTexture
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		EPixelFormat Format = PF_Unknown;
		/** Mip 0: GetPixelFormatDataSize bytes. */
		TArray<uint8> Data;
		/** Mips 1 and on: GetPixelFormatMipDataSize bytes each. */
		TArray<TArray<uint8>> Mips;
	};

	/**
	 * Turns Width x Height RGBA8 texels (bottom row first; sRGB colours when bSRGB) into a texture's paletted platform
	 * data; false leaves them RGBA8.
	 */
	using FTextureConverter = bool (*)(
		const uint8* Rgba, int32 Width, int32 Height, bool bSRGB, FConvertedTexture& Out);

	/** The converter of the assets' RGBA8 textures (null: none). Empties the cache. */
	void SetTextureConverter(FTextureConverter InConverter);

	/** The arena: NumBlocks 64-word blocks from FirstBlock (page aligned). Empties the cache. */
	void SetArena(uint32 FirstBlock, uint32 NumBlocks);
	[[nodiscard]] bool HasArena() const
	{
		return ArenaBlocks > 0;
	}

	/** The KB of texels and CLUTs a frame may upload (0: no limit). */
	void SetUploadBudgetKB(int32 InBudgetKB)
	{
		UploadBudgetBytes = FMath::Max(InBudgetKB, 0) * 1024;
	}

	/** Starts a frame: the textures it binds are kept until the next one, and the frame's counters restart. */
	void BeginFrame();

	/**
	 * Binds Texture (RGB from the texels, the CLUT's alpha for a paletted one, MODULATE): uploads what is not resident
	 * into List first (the upload budget permitting) and fills OutBinding, whose TEX0 the caller writes next. False
	 * when it cannot be sampled at all: no arena, no valid texels.
	 */
	bool BindTexture(const UTexture2D& Texture, FGSCommandList& List, FGSTextureBinding& OutBinding);

	/**
	 * TEX0 for texels the renderer makes (Key identifies them): Width x Height RGBA8, bottom row first, with their
	 * alpha (0..255 read as 0..0x80, 1.0 for the GS) when bAlpha. Level 0 only, outside the upload budget (they are
	 * few and small). False without an arena or room.
	 */
	bool BindTexels(const void* Key, int32 Width, int32 Height, TArrayView<const uint8> Rgba, bool bAlpha,
		FGSCommandList& List, FGSTex0& OutTex0);

	/**
	 * Forgets an asset's texture (its data changed or it is going away) and frees its blocks. True when it was
	 * resident: a list may still hold its data in place, so the caller copies what the lists not yet sent hold
	 * (FGSCommandList::CopyInPlaceImages) and waits for the one being sent.
	 */
	bool Release(const void* Key);
	/** Forgets every texture. */
	void Reset();

	[[nodiscard]] bool IsResident(const void* Key) const
	{
		return Entries.Contains(Key);
	}
	[[nodiscard]] int32 GetNumResident() const
	{
		return Entries.Num();
	}
	/** The blocks the resident textures take (256 bytes each). */
	[[nodiscard]] uint32 GetResidentBlocks() const
	{
		return UsedBlocks;
	}

	/** This frame's counters (since BeginFrame). */
	struct FFrameCounters
	{
		/** Textures uploaded, whole or in part. */
		int32 Uploads = 0;
		/** Bytes of texels and CLUTs uploaded. */
		int32 UploadBytes = 0;
		/** Textures evicted to make room. */
		int32 Evictions = 0;
		/** TEX0 writes that load the CLUT buffer (CLD 2 / 3, or 4 / 5 with another CLUT in it). */
		int32 ClutLoads = 0;
		/** Binds drawn with a fallback: the smallest level, or a flat colour. */
		int32 Deferred = 0;
	};
	[[nodiscard]] const FFrameCounters& GetFrameCounters() const
	{
		return Counters;
	}

private:
	/** A resident texture: its run of blocks and how it is sampled. */
	struct FEntry
	{
		/** The run, relative to the arena's first block. */
		uint32 FirstBlock = 0;
		uint32 NumBlocks = 0;
		/** TEX0 of level 0 (CLD and CSA are set at each bind). */
		FGSTex0 Tex0;
		FGSTextureLayout::FFootprint Footprint;
		/** The levels uploaded: all of them, or only the last (FirstResidentLevel). */
		int32 FirstResidentLevel = 0;
		/** When a frame last bound it (FrameNumber), and the bind's order (for the least recently used). */
		uint32 LastFrame = 0;
		uint64 LastUse = 0;
	};

	/** A texture's texels as the GS takes them: paletted levels and a palette, or PSMCT32 level 0. */
	struct FTexels
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		EGSPixelFormat Psm = EGSPixelFormat::PSMCT32;
		int32 NumLevels = 1;
		/** Each level's texels as uploaded (PSMT8 / PSMT4 indices or PSMCT32 words). */
		const uint8* Levels[FGSTextureLayout::MaxLevels] = {};
		/** The CLUT image as the GS reads it (a paletted format: CSM1, PSMCT32, alpha 0..0x80). */
		const uint8* Clut = nullptr;
		/** PSMCT32 texels: the texel's alpha is used (TCC). */
		bool bAlpha = false;
		/** The levels and the CLUT outlive the frame (an asset's data): uploaded in place, not copied. */
		bool bInPlace = false;
	};

	/** What the GS's CLUT buffer holds for CBP0 (slot 0) and CBP1 (slot 1). */
	struct FClutSlot
	{
		/** The register's value is known (the cache wrote it since the last reset). */
		bool bKnown = false;
		/** The buffer holds the CLUT at Cbp (its entries at this slot's CSA are that CLUT's). */
		bool bLoaded = false;
		/** The CLUT is a PSMT8 one (all 256 entries). */
		bool bIndex8 = false;
		uint16 Cbp = 0;
	};

	/** An asset's texels as the converter made them (kept across evictions). */
	using FConverted = FConvertedTexture;

	/**
	 * The texels of an asset (or its conversion) from its mips' data (MipData, locked by the caller); false when they
	 * cannot be sampled.
	 */
	bool GetTexels(
		const UTexture2D& Texture, const uint8* const* MipData, FTexels& OutTexels, TArray<uint8>& OutScratch);
	/** Binds an entry's texture, allocating and uploading it first (or part of it) as the budget allows. */
	bool Bind(
		const void* Key, const FTexels& Texels, bool bBudgeted, FGSCommandList& List, FGSTextureBinding& OutBinding);
	/** Uploads levels [FirstLevel, LastLevel] of an entry, and its CLUT when bClut. */
	void UploadLevels(const FEntry& Entry, const FTexels& Texels, int32 FirstLevel, int32 LastLevel, bool bClut,
		FGSCommandList& List);
	/** The bytes the upload of levels [FirstLevel, LastLevel] (and the CLUT when bClut) moves. */
	[[nodiscard]] static int32 GetUploadBytes(const FTexels& Texels, int32 FirstLevel, int32 LastLevel, bool bClut);
	/** Whether a frame that uploaded UploadBytes so far may upload Bytes more. */
	[[nodiscard]] bool FitsBudget(int32 Bytes) const;
	/** Fills the binding of an entry: TEX0 for its resident levels, MIPTBP1 / MIPTBP2, and the CLUT's load control. */
	void MakeBinding(const FEntry& Entry, FGSTextureBinding& OutBinding);
	/** The texels' average colour (their smallest level), for a flat stand-in. */
	[[nodiscard]] static FColor GetAverageColor(const FTexels& Texels);

	/**
	 * Takes a run of NumBlocks blocks starting at a multiple of Alignment (relative to the arena), evicting the least
	 * recently used entries the frame has not bound until one fits; false when none can.
	 */
	bool Allocate(uint32 NumBlocks, uint32 Alignment, uint32& OutFirstBlock);
	/** The first free run of NumBlocks at a multiple of Alignment; false when there is none. */
	[[nodiscard]] bool FindFreeRun(uint32 NumBlocks, uint32 Alignment, uint32& OutFirstBlock) const;
	void MarkBlocks(uint32 FirstBlock, uint32 NumBlocks, bool bUsed);
	/** Frees an entry's blocks and forgets it. */
	void Evict(const void* Key);
	/** Forgets the CLUTs the blocks [FirstBlock, FirstBlock + NumBlocks) held (absolute block numbers): rewritten. */
	void InvalidateClutSlots(uint32 FirstBlock, uint32 NumBlocks);

	TMap<const void*, FEntry> Entries;
	FTextureConverter Converter = nullptr;
	TMap<const void*, FConverted> Converted;
	uint32 ArenaFirst = 0;
	uint32 ArenaBlocks = 0;
	/** A bit a block of the arena: taken. */
	TArray<uint32> BlockBits;
	uint32 UsedBlocks = 0;
	FClutSlot ClutSlots[2];
	/** The PSMT4 slot bound last (the other is the one to replace; the first goes to slot 1, clear of PSMT8's). */
	int32 LastIndex4Slot = 0;
	int32 UploadBudgetBytes = 0;
	uint32 FrameNumber = 1;
	uint64 UseCounter = 0;
	FFrameCounters Counters;
};
