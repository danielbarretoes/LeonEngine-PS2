#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "RHIHandles.h"

class UStaticMesh;

/**
 * The GPU buffers of a UStaticMesh (UE: FStaticMeshRenderData): its LOD resources' vertices, with tangents computed at
 * upload, and indices in one VAO, drawn whole or by section. FRenderResourceCache makes it the first time the mesh is
 * drawn.
 */
class FStaticMeshRenderData
{
public:
	explicit FStaticMeshRenderData(const UStaticMesh& Mesh);
	~FStaticMeshRenderData();

	FStaticMeshRenderData(const FStaticMeshRenderData&) = delete;
	FStaticMeshRenderData& operator=(const FStaticMeshRenderData&) = delete;

	void Draw() const;
	void DrawSubMesh(int32 SubMeshIndex) const;

	[[nodiscard]] bool Valid() const
	{
		return IndexCount > 0 && Vao != InvalidVertexArray;
	}

private:
	FRHIVertexArrayId Vao = InvalidVertexArray;
	FRHIBufferId Vbo = InvalidBuffer;
	FRHIBufferId Ebo = InvalidBuffer;
	int32 IndexCount = 0;
	TArray<FMeshSection> Submeshes;
};
