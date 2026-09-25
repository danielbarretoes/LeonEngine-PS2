#include "Engine/StaticMesh.h"

namespace
{

	void ComputeLocalBounds(const FMeshData& Data, FVector& OutMin, FVector& OutMax)
	{
		OutMin = FVector(TNumericLimits<float>::Max());
		OutMax = FVector(TNumericLimits<float>::Lowest());
		for (const FVertex& Vertex : Data.Vertices)
		{
			OutMin = OutMin.ComponentMin(Vertex.Position);
			OutMax = OutMax.ComponentMax(Vertex.Position);
		}
	}

} // namespace

UStaticMesh UStaticMesh::CreateCpu(const FMeshData& Data)
{
	UStaticMesh Mesh;
	if (Data.IsEmpty())
	{
		return Mesh;
	}

	ComputeLocalBounds(Data, Mesh.LocalMin, Mesh.LocalMax);
	Mesh.IndexCount = Data.Indices.Num();
	Mesh.Materials = Data.Materials;
	if (Data.Submeshes.Num() == 0)
	{
		Mesh.Submeshes.Add(FMeshSection{0, Mesh.IndexCount, 0});
	}
	else
	{
		Mesh.Submeshes = Data.Submeshes;
	}
	Mesh.CpuData = Data;
	return Mesh;
}
