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

/** Orthonormalizes tangents from triangle UVs (needed for normal mapping). */
void ComputeTangents(FMeshData& Data);
