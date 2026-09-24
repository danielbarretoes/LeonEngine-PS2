#pragma once

#include "Material.h"
#include "MeshData.h"
#include "RHIHandles.h"

#include <glm/vec3.hpp>

#include <vector>

/// GPU static mesh resource (Unreal-style UStaticMesh; VAO/VBO/EBO + optional MTL).
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
	/// Bounds + materials only (no VAO). For dedicated / headless simulation.
	[[nodiscard]] static UStaticMesh CreateCpu(const FMeshData& Data);

	void Draw() const;
	void DrawSubMesh(std::size_t SubMeshIndex) const;

	[[nodiscard]] bool Valid() const
	{
		return IndexCount > 0 && (bCpuOnly || Vao != InvalidVertexArray);
	}
	[[nodiscard]] bool IsCpuOnly() const
	{
		return bCpuOnly;
	}
	[[nodiscard]] int GetIndexCount() const
	{
		return IndexCount;
	}
	[[nodiscard]] int TriangleCount() const
	{
		return IndexCount / 3;
	}
	[[nodiscard]] const glm::vec3& GetLocalMin() const
	{
		return LocalMin;
	}
	[[nodiscard]] const glm::vec3& GetLocalMax() const
	{
		return LocalMax;
	}
	[[nodiscard]] const std::vector<FMeshSection>& GetSubmeshes() const
	{
		return Submeshes;
	}
	[[nodiscard]] const std::vector<FMaterial>& GetMaterials() const
	{
		return Materials;
	}
	[[nodiscard]] bool HasMaterials() const
	{
		return !Materials.empty();
	}
	/// CPU copy retained for lightmap bake / editor tools (empty if upload had no data).
	[[nodiscard]] const FMeshData& GetCpuData() const
	{
		return CpuData;
	}
	[[nodiscard]] bool HasCpuData() const
	{
		return !CpuData.empty();
	}

private:
	void Destroy();

	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int IndexCount = 0;
	bool bCpuOnly = false;
	glm::vec3 LocalMin{0.0f};
	glm::vec3 LocalMax{0.0f};
	std::vector<FMeshSection> Submeshes;
	std::vector<FMaterial> Materials;
	FMeshData CpuData{};
};
