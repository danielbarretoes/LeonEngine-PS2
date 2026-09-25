#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "Vertex.h"

/** Contiguous index range drawn with one material slot. */
struct RENDERCORE_API FMeshSection
{
	int32 IndexOffset = 0; // in indices (not bytes)
	int32 IndexCount = 0;
	int32 MaterialIndex = 0;
};

/**
 * CPU-side mesh data (no OpenGL handles): what the importers (MeshUtilities) read from a source file and UStaticMesh
 * builds from. Materials and the arrays after it are parallel, one entry per material slot; the static mesh factories
 * make a UMaterial asset of each named slot.
 */
struct RENDERCORE_API FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FMeshSection> Submeshes;
	/** Each slot's material values from the source (the OBJ `.mtl`, the glTF material). */
	TArray<FMaterial> Materials;
	/** Each slot's material name in the source; empty for a slot the source gives no material. */
	TArray<FString> MaterialSlotNames;
	/** Each slot's base colour map: the image file the source names (absolute), or empty. */
	TArray<FString> AlbedoMapPaths;
	/** Each slot's normal map: the image file the source names (absolute), or empty. */
	TArray<FString> NormalMapPaths;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}
};

/** The world a mesh's data is in: it picks the fallback tangent of vertices without a UV gradient. */
enum class EMeshDataBasis : uint8
{
	/** The engine world (UE: Z up, left-handed). */
	Engine,
	/** The legacy Y-up, right-handed world of the old cooked meshes (tests compare against it). */
	LegacyYUp,
};

/**
 * Orthonormalizes tangents from triangle UVs (needed for normal mapping). A vertex without a UV gradient gets a tangent
 * across the world up of Basis; in the engine basis it is the converted legacy fallback.
 */
void ComputeTangents(FMeshData& Data, EMeshDataBasis Basis = EMeshDataBasis::Engine);
