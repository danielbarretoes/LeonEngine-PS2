#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "MeshData.h"

/**
 * A static mesh asset (UE: UStaticMesh): the CPU mesh data with its bounds, sections and materials. The renderer
 * builds its GPU buffers from it the first time it draws it (its own render resource cache: Engine never sees GPU
 * objects), and the physics scene reads its triangles for static collision.
 *
 * Plain C++ shared through TSharedPtr and FResourceCache until P14 makes it a UObject asset loaded with LoadObject.
 */
class ENGINE_API UStaticMesh
{
public:
	/** The asset of Data (UE: built from its mesh description); an empty mesh is not Valid. */
	[[nodiscard]] static UStaticMesh CreateCpu(const FMeshData& Data);

	[[nodiscard]] bool Valid() const
	{
		return IndexCount > 0;
	}
	[[nodiscard]] int32 GetIndexCount() const
	{
		return IndexCount;
	}
	[[nodiscard]] int32 TriangleCount() const
	{
		return IndexCount / 3;
	}
	[[nodiscard]] const FVector& GetLocalMin() const
	{
		return LocalMin;
	}
	[[nodiscard]] const FVector& GetLocalMax() const
	{
		return LocalMax;
	}
	/** The sections (one covering every index when the data had none). */
	[[nodiscard]] const TArray<FMeshSection>& GetSubmeshes() const
	{
		return Submeshes;
	}
	[[nodiscard]] const TArray<FMaterial>& GetMaterials() const
	{
		return Materials;
	}
	[[nodiscard]] bool HasMaterials() const
	{
		return Materials.Num() > 0;
	}
	/** The mesh data the asset was made from (vertices, indices, sections); the renderer uploads it. */
	[[nodiscard]] const FMeshData& GetCpuData() const
	{
		return CpuData;
	}
	[[nodiscard]] bool HasCpuData() const
	{
		return !CpuData.IsEmpty();
	}

private:
	int32 IndexCount = 0;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	TArray<FMeshSection> Submeshes;
	TArray<FMaterial> Materials;
	FMeshData CpuData{};
};
