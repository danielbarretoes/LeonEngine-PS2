#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "Vertex.h"

/**
 * The geometry of a static mesh as the renderer and the physics scene read it (UE: FStaticMeshLODResources, one LOD):
 * interleaved vertices, 32-bit indices and the sections, each a range of indices drawn with one material slot. A
 * UStaticMesh keeps it on the CPU and saves it as bulk data; the renderer uploads its own GPU copy.
 */
struct ENGINE_API FStaticMeshLODResources
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	/** At least one: a mesh built without sections gets one over every index. */
	TArray<FMeshSection> Sections;

	[[nodiscard]] int32 GetNumVertices() const
	{
		return Vertices.Num();
	}
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return Indices.Num() / 3;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}

	/** The vertices, indices and sections as FMeshData (the renderer computes the tangents on it when it uploads). */
	void ToMeshData(FMeshData& OutData) const;

	/** Loads or saves the arrays: the payload of UStaticMesh's bulk data (little-endian, field by field). */
	void Serialize(FArchive& Ar);
};
