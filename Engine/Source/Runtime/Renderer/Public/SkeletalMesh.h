#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "RHIHandles.h"
#include "SkeletalAnimation.h"

/** GPU skinned mesh (VAO with bone indices/weights). */
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
	/** Skeleton / bounds / index count only (no VAO). Headless path. */
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
	[[nodiscard]] int32 GetIndexCount() const
	{
		return IndexCount;
	}
	[[nodiscard]] int32 TriangleCount() const
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
	[[nodiscard]] const FVector& GetLocalMin() const
	{
		return LocalMin;
	}
	[[nodiscard]] const FVector& GetLocalMax() const
	{
		return LocalMax;
	}
	/** Uniform scale that makes the mesh FitHeight tall (its Z extent, world units). */
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
		Material = MoveTemp(InMaterial);
	}

private:
	void Destroy();

	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int32 IndexCount = 0;
	bool bCpuOnly = false;
	USkeleton Skeleton{};
	UAnimSequence EmbeddedAnim{};
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	FMaterial Material{};
};
