#pragma once

#include "CoreMinimal.h"
#include "LPS2Mesh.h"

/**
 * The geometry of a static mesh as the renderer reads it (UE: FStaticMeshLODResources, one LOD): LPS2 v2, the
 * quantized triangle strips in batches for VU1 (FLPS2Mesh, Docs/ASSET_FORMATS.md), on every platform. MeshUtilities
 * builds it when the mesh is imported (UStaticMesh::BuildFromMeshData); a UStaticMesh keeps it on the CPU and saves it
 * as bulk data, and the GS scene renderer draws its strips.
 */
struct ENGINE_API FStaticMeshLODResources
{
	FLPS2Mesh RenderData;

	/** The vertices of every batch (a vertex two strips share is in each). */
	[[nodiscard]] int32 GetNumVertices() const
	{
		return RenderData.GetNumVertices();
	}
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return RenderData.GetNumTriangles();
	}
	[[nodiscard]] int32 GetNumSections() const
	{
		return RenderData.GetNumSections();
	}
	/** The material slot of a section (0 for an index out of range). */
	[[nodiscard]] int32 GetSectionMaterialIndex(int32 SectionIndex) const
	{
		return SectionIndex >= 0 && SectionIndex < GetNumSections()
			? int32(RenderData.GetSection(SectionIndex).MaterialIndex)
			: 0;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return RenderData.IsEmpty() || RenderData.GetNumTriangles() == 0;
	}

	/** Loads or saves the render data: part of UStaticMesh's bulk data. */
	void Serialize(FArchive& Ar)
	{
		RenderData.Serialize(Ar);
	}
};

/**
 * The triangles a static mesh collides with (UE: FTriMeshCollisionData, which UE builds from the render data when it
 * cooks the physics mesh): the source's vertices at full precision, in the mesh's space (centimetres), and three
 * indices a triangle, in the source's order. The render data is quantized, so the physics scene (FPhysScene) reads
 * these instead. Leon keeps flat uint32 indices (UE: FTriIndices). Each triangle's material slot (UE: MaterialIndices)
 * gives a hit on it its material (UStaticMeshComponent::GetMaterialFromCollisionFaceIndex).
 */
struct ENGINE_API FTriMeshCollisionData
{
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	/** The material slot of each triangle (UE: MaterialIndices); as many as the triangles. */
	TArray<uint16> MaterialIndices;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() < 3;
	}

	/** Loads or saves the arrays: part of UStaticMesh's bulk data. */
	void Serialize(FArchive& Ar);
};
