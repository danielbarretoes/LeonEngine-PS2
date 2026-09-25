#include "Engine/SkeletalMesh.h"

USkeletalMesh USkeletalMesh::CreateCpu(FSkeletalMeshData Data)
{
	USkeletalMesh Mesh;
	if (Data.IsEmpty())
	{
		return Mesh;
	}
	Mesh.Skeleton = MoveTemp(Data.Skeleton);
	Mesh.EmbeddedAnim = MoveTemp(Data.EmbeddedAnim);
	Mesh.LocalMin = Data.LocalMin;
	Mesh.LocalMax = Data.LocalMax;
	Mesh.IndexCount = Data.Indices.Num();
	Mesh.Vertices = MoveTemp(Data.Vertices);
	Mesh.Indices = MoveTemp(Data.Indices);
	return Mesh;
}

float USkeletalMesh::FitUniformScale(float FitHeight) const
{
	if (FitHeight <= 0.0f)
	{
		return 1.0f;
	}
	/** 0.1 cm: keeps a flat mesh from dividing by zero. The height is the Z extent. */
	const float Height = FMath::Max((LocalMax - LocalMin).Z, 0.1f);
	return FitHeight / Height;
}
