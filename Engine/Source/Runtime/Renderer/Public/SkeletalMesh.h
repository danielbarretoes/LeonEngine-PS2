#pragma once

#include "Material.h"
#include "RHIHandles.h"
#include "SkeletalAnimation.h"

#include <glm/vec3.hpp>

#include <memory>
#include <vector>

/// GPU skinned mesh (VAO with bone indices/weights).
class RENDERER_API USkeletalMesh
{
public:
	USkeletalMesh() = default;
	~USkeletalMesh();

	USkeletalMesh(const USkeletalMesh&) = delete;
	USkeletalMesh& operator=(const USkeletalMesh&) = delete;
	USkeletalMesh(USkeletalMesh&& Other) noexcept;
	USkeletalMesh& operator=(USkeletalMesh&& Other) noexcept;

	[[nodiscard]] static USkeletalMesh Upload(FSkeletalMeshData Data);
	/// Skeleton / bounds / index count only (no VAO). Dedicated server path.
	[[nodiscard]] static USkeletalMesh CreateCpu(FSkeletalMeshData Data);

	void Draw() const;

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
	[[nodiscard]] const USkeleton& GetSkeleton() const
	{
		return Skeleton;
	}
	[[nodiscard]] const UAnimSequence& GetEmbeddedAnim() const
	{
		return EmbeddedAnim;
	}
	[[nodiscard]] const glm::vec3& GetLocalMin() const
	{
		return LocalMin;
	}
	[[nodiscard]] const glm::vec3& GetLocalMax() const
	{
		return LocalMax;
	}
	[[nodiscard]] float FitUniformScale(float FitHeight) const;

	[[nodiscard]] FMaterial& GetMaterial()
	{
		return Material;
	}
	[[nodiscard]] const FMaterial& GetMaterial() const
	{
		return Material;
	}
	void SetMaterial(FMaterial InMaterial)
	{
		Material = std::move(InMaterial);
	}

private:
	void Destroy();

	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int IndexCount = 0;
	bool bCpuOnly = false;
	USkeleton Skeleton{};
	UAnimSequence EmbeddedAnim{};
	glm::vec3 LocalMin{0.0f};
	glm::vec3 LocalMax{0.0f};
	FMaterial Material{};
};
