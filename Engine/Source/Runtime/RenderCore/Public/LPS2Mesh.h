#pragma once

#include "CoreMinimal.h"

/** The header of an LPS2 v2 blob: three quadwords (Docs/ASSET_FORMATS.md, LPS2 v2). */
struct FLPS2MeshHeader
{
	/** FLPS2Mesh::Magic: the bytes "LPS2". */
	uint32 Magic = 0;
	/** FLPS2Mesh::Version. */
	uint16 Version = 0;
	/** FLPS2Mesh::FlagSkinned for a skinned mesh's blob; 0 for a static mesh's. */
	uint16 Flags = 0;
	uint32 NumSections = 0;
	uint32 NumBatches = 0;
	/** A position is Quantized * PositionScale + PositionBias, per axis, in the mesh's space (centimetres). */
	float PositionScale[3] = {0.0f, 0.0f, 0.0f};
	/** The vertices of every batch (a vertex two strips or two batches share is in each). */
	uint32 NumVertices = 0;
	float PositionBias[3] = {0.0f, 0.0f, 0.0f};
	/** The triangles the strips draw: the source's, without its degenerate ones. */
	uint32 NumTriangles = 0;
};
static_assert(sizeof(FLPS2MeshHeader) == 48, "The LPS2 v2 header is three quadwords");

/** A section: a run of batches drawn with one material slot (one quadword). */
struct FLPS2Section
{
	uint32 FirstBatch = 0;
	uint32 NumBatches = 0;
	/** The material slot (UStaticMesh::StaticMaterials). */
	uint32 MaterialIndex = 0;
	uint32 NumTriangles = 0;
};
static_assert(sizeof(FLPS2Section) == 16, "An LPS2 v2 section is one quadword");

/**
 * A batch: the vertices of a run of triangle strips, as many as VU1's double-buffered data memory takes
 * (FLPS2Mesh::MaxBatchVertices), and what the VU needs besides them (two quadwords).
 */
struct FLPS2Batch
{
	/** Where the batch's streams start, in bytes from the start of the blob: a multiple of 16. */
	uint32 DataOffset = 0;
	/** 3 to FLPS2Mesh::MaxBatchVertices. */
	uint32 NumVertices = 0;
	/** Whole repeats added to the batch's texture coordinates (so that the 4.12 values stay within +-8). */
	float TexCoordOffset[2] = {0.0f, 0.0f};
	/** The sphere around the batch's positions, in the mesh's space: what decides culling and clipping. */
	float BoundsCenter[3] = {0.0f, 0.0f, 0.0f};
	float BoundsRadius = 0.0f;
};
static_assert(sizeof(FLPS2Batch) == 32, "An LPS2 v2 batch is two quadwords");

/**
 * The bone palette of a skinned batch, the first two quadwords of its data: the skeleton's bones its vertices'
 * palette indices name. The EE puts their matrices in VU1's batch header (3 quadwords a bone).
 */
struct FLPS2SkinPalette
{
	/** Bone indices of the skeleton; the first NumBones are used. */
	uint8 Bones[24] = {};
	uint32 NumBones = 0;
	uint32 Padding = 0;
};
static_assert(sizeof(FLPS2SkinPalette) == 32, "A skinned batch's palette is two quadwords");

/**
 * The render data of a static or skinned mesh in the shape the PS2's VU1 reads it: LPS2 v2 (Docs/ASSET_FORMATS.md, LPS2
 * v2; Docs/PLANS/ps2-shipping.md D1: one format on every platform, which the C++ emitter reads on the PC and in the
 * tests, and VU1 on the PS2). One little-endian blob, every part of it on a quadword (16 bytes):
 *
 * - the header, the section table and the batch table;
 * - each batch's streams, each padded to a quadword so that a VIF UNPACK takes it as it is: the positions (V3-16,
 *   int16 x 3 with the header's scale and bias), the normals (V4-8, int8 x 3 times 127, and the strip flags in W), the
 *   baked colours (V4-8 unsigned, RGBA8) and the texture coordinates (V2-16, 4.12 fixed point plus the batch's whole
 *   offset);
 * - a skinned mesh's blob (FlagSkinned in the header) starts each batch's data with its bone palette
 *   (FLPS2SkinPalette) and ends it with a fifth stream, the skin (V4-8 unsigned: two palette indices, then their two
 *   weights in 1/255 steps that add up to 255); its positions and normals are the bind pose's.
 *
 * A batch's vertices are triangle strips one after the other: vertex i closes the triangle of vertices i - 2, i - 1
 * and i unless its flags have FlagNoKick (a strip's first two vertices, a degenerate triangle: the GS's ADC, XYZ3), and
 * that triangle is wound as the source's (i - 1, i - 2, i) when they have FlagReversed. MeshUtilities' FLPS2MeshBuilder
 * makes it at import (edit time); the runtime only reads it.
 */
class RENDERCORE_API FLPS2Mesh
{
public:
	FLPS2Mesh() = default;
	FLPS2Mesh(const FLPS2Mesh& Other) = default;
	FLPS2Mesh(FLPS2Mesh&& Other) = default;
	/**
	 * The blob a mesh gives up (destroyed, assigned or loaded over) goes through FRHIDeferredRelease: the PS2's frame
	 * in flight may still read it by DMA (Docs/PLANS/ps2-shipping.md N14).
	 */
	~FLPS2Mesh();
	FLPS2Mesh& operator=(const FLPS2Mesh& Other);
	FLPS2Mesh& operator=(FLPS2Mesh&& Other);

	/** "LPS2" in the blob's first bytes. */
	static constexpr uint32 Magic = 0x3253504Cu;
	static constexpr uint16 Version = 2;
	/** The header's flag of a skinned mesh's blob. */
	static constexpr uint16 FlagSkinned = 0x0001;

	/**
	 * The most vertices of a batch. VU1's 1024 quadwords of data memory: 32 for the constants, two buffers of 496 for
	 * the double buffering; a buffer takes a batch's 2 header quadwords, 4 quadwords a vertex unpacked (position,
	 * normal, colour, texture coordinates) and, for the GIF, a tag and 3 quadwords a vertex (ST, RGBAQ, XYZ2):
	 * 3 + 7 x 64 = 451 <= 496.
	 */
	static constexpr int32 MaxBatchVertices = 64;
	/**
	 * The most vertices of a skinned batch: its header also holds its palette's matrices (3 quadwords a bone, 72) and
	 * a vertex unpacks to 5 quadwords (the skin too): 3 + 72 + 8 x 48 = 459 <= 496.
	 */
	static constexpr int32 MaxSkinnedBatchVertices = 48;
	/** The most bones a skinned batch's palette holds. */
	static constexpr int32 MaxPaletteBones = 24;
	/** The bytes of a skinned batch's palette (FLPS2SkinPalette). */
	static constexpr int32 PaletteBytes = 32;
	/** The largest quantized position component: the scale spreads each axis's half extent over it. */
	static constexpr int32 PositionRange = 32767;
	/** A normal component is its value times this. */
	static constexpr int32 NormalRange = 127;
	/** Texture coordinates are 4.12 fixed point: a repeat is this many units. */
	static constexpr int32 TexCoordOne = 4096;

	/** Strip flags, in a normal's W (its sign extended word has bit 15 set: the PACKED XYZ2's ADC bit). */
	static constexpr uint8 FlagNoKick = 0x80;
	static constexpr uint8 FlagReversed = 0x01;

	/** The bytes of a batch's streams, each padded to a quadword. */
	[[nodiscard]] static constexpr int32 PositionBytes(int32 NumVertices)
	{
		return AlignQuadword(NumVertices * 6);
	}
	[[nodiscard]] static constexpr int32 NormalBytes(int32 NumVertices)
	{
		return AlignQuadword(NumVertices * 4);
	}
	[[nodiscard]] static constexpr int32 ColorBytes(int32 NumVertices)
	{
		return AlignQuadword(NumVertices * 4);
	}
	[[nodiscard]] static constexpr int32 TexCoordBytes(int32 NumVertices)
	{
		return AlignQuadword(NumVertices * 4);
	}
	[[nodiscard]] static constexpr int32 SkinBytes(int32 NumVertices)
	{
		return AlignQuadword(NumVertices * 4);
	}
	/** A batch's data: its streams, and a skinned batch's palette and skin. */
	[[nodiscard]] static constexpr int32 BatchBytes(int32 NumVertices, bool bSkinned = false)
	{
		return (bSkinned ? PaletteBytes + SkinBytes(NumVertices) : 0) + PositionBytes(NumVertices) +
			NormalBytes(NumVertices) + ColorBytes(NumVertices) + TexCoordBytes(NumVertices);
	}
	/** Where the batch table starts, for NumSections sections. */
	[[nodiscard]] static constexpr int32 BatchTableOffset(int32 NumSections)
	{
		return int32(sizeof(FLPS2MeshHeader)) + (NumSections * int32(sizeof(FLPS2Section)));
	}
	[[nodiscard]] static constexpr int32 AlignQuadword(int32 Bytes)
	{
		return (Bytes + 15) & ~15;
	}

	/** Takes a blob; false, leaving the mesh empty, when it is not a whole LPS2 v2 blob (IsValidBlob). */
	bool SetData(TArray<uint8>&& InData);
	/** The blob. */
	[[nodiscard]] const TArray<uint8>& GetData() const
	{
		return Data;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Data.Num() == 0;
	}

	/**
	 * True for a whole LPS2 v2 blob: the header, the tables and every batch's streams inside it and aligned; a skinned
	 * batch within its limits (vertices, palette bones, palette indices, weights).
	 */
	[[nodiscard]] static bool IsValidBlob(const TArray<uint8>& Blob);

	/** The header (zeroes for an empty mesh). */
	[[nodiscard]] const FLPS2MeshHeader& GetHeader() const;
	[[nodiscard]] int32 GetNumSections() const
	{
		return int32(GetHeader().NumSections);
	}
	[[nodiscard]] int32 GetNumBatches() const
	{
		return int32(GetHeader().NumBatches);
	}
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return int32(GetHeader().NumTriangles);
	}
	[[nodiscard]] int32 GetNumVertices() const
	{
		return int32(GetHeader().NumVertices);
	}
	/** A skinned mesh's blob: palettes and skin streams (FlagSkinned). */
	[[nodiscard]] bool IsSkinned() const
	{
		return (GetHeader().Flags & FlagSkinned) != 0;
	}
	[[nodiscard]] const FLPS2Section& GetSection(int32 Index) const;
	[[nodiscard]] const FLPS2Batch& GetBatch(int32 Index) const;

	/** A batch's streams: 3 int16 a vertex, 4 int8 (the fourth the flags), 4 uint8 (RGBA), 2 int16. */
	[[nodiscard]] const int16* GetPositions(const FLPS2Batch& Batch) const;
	[[nodiscard]] const int8* GetNormals(const FLPS2Batch& Batch) const;
	[[nodiscard]] const uint8* GetColors(const FLPS2Batch& Batch) const;
	[[nodiscard]] const int16* GetTexCoords(const FLPS2Batch& Batch) const;
	/** A skinned batch's palette and skin stream (4 bytes a vertex: palette indices 0 and 1, weights 0 and 1). */
	[[nodiscard]] const FLPS2SkinPalette& GetPalette(const FLPS2Batch& Batch) const;
	[[nodiscard]] const uint8* GetSkin(const FLPS2Batch& Batch) const;

	/** A quantized position in the mesh's space (centimetres). */
	[[nodiscard]] FVector DequantizePosition(const int16* Quantized) const
	{
		const FLPS2MeshHeader& Header = GetHeader();
		return FVector((float(Quantized[0]) * Header.PositionScale[0]) + Header.PositionBias[0],
			(float(Quantized[1]) * Header.PositionScale[1]) + Header.PositionBias[1],
			(float(Quantized[2]) * Header.PositionScale[2]) + Header.PositionBias[2]);
	}
	/** A quantized normal, not normalized. */
	[[nodiscard]] static FVector DequantizeNormal(const int8* Quantized)
	{
		constexpr float Inverse = 1.0f / float(NormalRange);
		return FVector(float(Quantized[0]) * Inverse, float(Quantized[1]) * Inverse, float(Quantized[2]) * Inverse);
	}
	/** A batch's 4.12 texture coordinates with its whole offset. */
	[[nodiscard]] static FVector2D DequantizeTexCoord(const int16* Quantized, const FLPS2Batch& Batch)
	{
		constexpr float Inverse = 1.0f / float(TexCoordOne);
		return FVector2D((float(Quantized[0]) * Inverse) + Batch.TexCoordOffset[0],
			(float(Quantized[1]) * Inverse) + Batch.TexCoordOffset[1]);
	}

	/**
	 * A section's texture coordinate density: texture repeats per centimetre of the mesh's space, the square root of
	 * its triangles' area in texture space over their area in the mesh's (0 without area). What the GS's level of
	 * detail starts from (Docs/PLANS/ps2-shipping.md N13); derived from the blob when it is set or loaded, not stored.
	 */
	[[nodiscard]] float GetSectionUvDensity(int32 Index) const
	{
		return SectionUvDensity.IsValidIndex(Index) ? SectionUvDensity[Index] : 0.0f;
	}

	/**
	 * Where a batch's colour stream starts in an instance's colour streams (FLPS2ColorStreams): after the colour
	 * streams of the batches before it, each padded to a quadword. Derived from the blob, not stored.
	 */
	[[nodiscard]] int32 GetColorStreamOffset(int32 BatchIndex) const
	{
		return ColorStreamOffsets.IsValidIndex(BatchIndex) ? ColorStreamOffsets[BatchIndex] : 0;
	}
	/** The bytes of every batch's colour stream: the size of an instance's colour streams. */
	[[nodiscard]] int32 GetColorStreamsSize() const
	{
		return ColorStreamOffsets.Num() > 0 ? ColorStreamOffsets.Last() : 0;
	}
	/** The CRC-32 of the blob (FCrc::MemCrc32): the mesh an instance's colour streams were made for. Derived. */
	[[nodiscard]] uint32 GetDataCrc() const
	{
		return DataCrc;
	}

	/** Loads or saves the blob (int32 size, then the bytes); a damaged blob sets the archive's error. */
	void Serialize(FArchive& Ar);

private:
	/** Fills the derived data (SectionUvDensity, ColorStreamOffsets, DataCrc) from the blob. */
	void ComputeDerivedData();
	/** Hands the blob to FRHIDeferredRelease (the mesh is empty after). */
	void ReleaseData();
	/** Fills SectionUvDensity from the blob's strips. */
	void ComputeUvDensity();
	/** Where a batch's streams start: after a skinned batch's palette. */
	[[nodiscard]] int32 GetStreamsOffset(const FLPS2Batch& Batch) const
	{
		return int32(Batch.DataOffset) + (IsSkinned() ? PaletteBytes : 0);
	}

	TArray<uint8> Data;
	TArray<float> SectionUvDensity;
	/** Each batch's colour stream offset in an instance's colour streams, then their size (NumBatches + 1). */
	TArray<int32> ColorStreamOffsets;
	uint32 DataCrc = 0;
};

/**
 * An instance's own colour streams for an LPS2 v2 mesh (Docs/ASSET_FORMATS.md, LPS2 v2, an instance's colours; UE: the
 * OverrideVertexColors of a component's FStaticMeshComponentLODInfo): what LeonEd's lighting bake writes for a static
 * mesh placed in a map (Docs/PLANS/ps2-shipping.md N22). The instances of a mesh share its positions, normals and
 * texture coordinates, and each brings its own colours: every batch's colour stream in batch order, each as the mesh's
 * (RGBA8 a vertex, 255 = 1, padded to a quadword), so VU1 can UNPACK an instance's colours by reference in place of the
 * mesh's. They are made for one blob (MeshCrc): a mesh rebuilt since does not take them.
 */
struct RENDERCORE_API FLPS2ColorStreams
{
	FLPS2ColorStreams() = default;
	FLPS2ColorStreams(const FLPS2ColorStreams& Other) = default;
	FLPS2ColorStreams(FLPS2ColorStreams&& Other) = default;
	/** Colours given up go through FRHIDeferredRelease: the PS2's frame in flight may read them by DMA (N14). */
	~FLPS2ColorStreams();
	FLPS2ColorStreams& operator=(const FLPS2ColorStreams& Other);
	FLPS2ColorStreams& operator=(FLPS2ColorStreams&& Other);

	/** FLPS2Mesh::GetDataCrc of the mesh the colours were made for. */
	uint32 MeshCrc = 0;
	/** Every batch's colours at FLPS2Mesh::GetColorStreamOffset: FLPS2Mesh::GetColorStreamsSize bytes. */
	TArray<uint8> Data;

	[[nodiscard]] bool IsEmpty() const
	{
		return Data.Num() == 0;
	}
	/** Sizes the streams for Mesh, every vertex white (1, as the mesh's own colours), made for it. */
	void Init(const FLPS2Mesh& Mesh);
	/** Empties them (the colours go through FRHIDeferredRelease). */
	void Reset();
	/** True when the colours were made for Mesh as it is. */
	[[nodiscard]] bool Matches(const FLPS2Mesh& Mesh) const;
	/** The RGBA8 colours of a batch of the mesh they match (4 bytes a vertex). */
	[[nodiscard]] uint8* GetBatchColors(const FLPS2Mesh& Mesh, int32 BatchIndex)
	{
		return Data.GetData() + Mesh.GetColorStreamOffset(BatchIndex);
	}
	[[nodiscard]] const uint8* GetBatchColors(const FLPS2Mesh& Mesh, int32 BatchIndex) const
	{
		return Data.GetData() + Mesh.GetColorStreamOffset(BatchIndex);
	}

	/** Loads or saves them (uint32 MeshCrc, int32 size, the bytes); a size that is not whole quadwords sets the error.
	 */
	void Serialize(FArchive& Ar);
};
