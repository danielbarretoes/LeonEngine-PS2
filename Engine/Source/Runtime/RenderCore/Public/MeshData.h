#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "Vertex.h"

/** Contiguous index range drawn with one material slot. */
struct RENDERCORE_API FMeshSection
{
	int32 IndexOffset = 0; // in indices (not bytes)
	int32 IndexCount = 0;
	int32 MaterialIndex = 0;
};

/** CPU-side mesh asset (no OpenGL handles). */
struct RENDERCORE_API FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FMeshSection> Submeshes;
	TArray<FMaterial> Materials;
	/** Parallel to Materials; resolved to FMaterial::AlbedoMap by FResourceCache. */
	TArray<FString> AlbedoMapPaths;

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
	/** The legacy Y-up, right-handed world the importers still produce (their output is converted at load). */
	LegacyYUp,
};

/**
 * Orthonormalizes tangents from triangle UVs (needed for normal mapping). A vertex without a UV gradient gets a tangent
 * across the world up of Basis; in the engine basis it is the converted legacy fallback.
 */
void ComputeTangents(FMeshData& Data, EMeshDataBasis Basis = EMeshDataBasis::Engine);
