#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "SkeletalAnimation.h"

/**
 * A skinned mesh asset (UE: USkeletalMesh): the skeleton, the embedded animation, the skinned vertices and indices, the
 * bounds and the material. The renderer builds its GPU buffers from it the first time it draws it.
 *
 * Plain C++ shared through TSharedPtr until P14 makes it a UObject asset.
 */
class ENGINE_API USkeletalMesh
{
public:
	/** The asset of Data; an empty mesh is not Valid. */
	[[nodiscard]] static USkeletalMesh CreateCpu(FSkeletalMeshData Data);

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

	/** The skinned vertices and the indices the renderer uploads. */
	[[nodiscard]] const TArray<FSkeletalVertex>& GetVertices() const
	{
		return Vertices;
	}
	[[nodiscard]] const TArray<uint32>& GetIndices() const
	{
		return Indices;
	}

private:
	int32 IndexCount = 0;
	USkeleton Skeleton{};
	UAnimSequence EmbeddedAnim{};
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	FMaterial Material{};
	TArray<FSkeletalVertex> Vertices;
	TArray<uint32> Indices;
};
