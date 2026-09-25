#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "MeshData.h"
#include "RHIHandles.h"

/** GPU static mesh resource (Unreal-style UStaticMesh; VAO/VBO/EBO + optional MTL). */
class RENDERER_API UStaticMesh
{
public:
	UStaticMesh() = default;
	~UStaticMesh();

	UStaticMesh(const UStaticMesh&) = delete;
	UStaticMesh& operator=(const UStaticMesh&) = delete;
	UStaticMesh(UStaticMesh&& Other) noexcept;
	UStaticMesh& operator=(UStaticMesh&& Other) noexcept;

	[[nodiscard]] static UStaticMesh Upload(const FMeshData& Data);
	/** Bounds + materials only (no VAO). For dedicated / headless simulation. */
	[[nodiscard]] static UStaticMesh CreateCpu(const FMeshData& Data);

	void Draw() const;
	void DrawSubMesh(int32 SubMeshIndex) const;

	[[nodiscard]] bool Valid() const
	{
		return IndexCount > 0 && (bCpuOnly || Vao != InvalidVertexArray);
	}
	[[nodiscard]] bool IsCpuOnly() const
	{
		return bCpuOnly;
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
	/** CPU copy retained for editor tools (empty if the upload had no data). */
	[[nodiscard]] const FMeshData& GetCpuData() const
	{
		return CpuData;
	}
	[[nodiscard]] bool HasCpuData() const
	{
		return !CpuData.IsEmpty();
	}

private:
	void Destroy();

	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int32 IndexCount = 0;
	bool bCpuOnly = false;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	TArray<FMeshSection> Submeshes;
	TArray<FMaterial> Materials;
	FMeshData CpuData{};
};
