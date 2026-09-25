#pragma once

#include "CoreMinimal.h"
#include "RHIHandles.h"

class USkeletalMesh;

/**
 * The GPU buffers of a USkeletalMesh (UE: FSkeletalMeshRenderData): skinned vertices (bone indices and weights) and
 * indices in one VAO. FRenderResourceCache makes it the first time the mesh is drawn.
 */
class FSkeletalMeshRenderData
{
public:
	explicit FSkeletalMeshRenderData(const USkeletalMesh& Mesh);
	~FSkeletalMeshRenderData();

	FSkeletalMeshRenderData(const FSkeletalMeshRenderData&) = delete;
	FSkeletalMeshRenderData& operator=(const FSkeletalMeshRenderData&) = delete;

	void Draw() const;

	[[nodiscard]] bool Valid() const
	{
		return IndexCount > 0 && Vao != InvalidVertexArray;
	}

private:
	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int32 IndexCount = 0;
};
