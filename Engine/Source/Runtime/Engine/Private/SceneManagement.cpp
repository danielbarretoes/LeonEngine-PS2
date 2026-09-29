#include "SceneManagement.h"

#include "Engine/StaticMesh.h"

float ComputeBoundsScreenSize(
	const FVector& BoundsOrigin, float SphereRadius, const FVector& ViewOrigin, const FMatrix& ProjMatrix)
{
	const float Distance = FVector::Dist(BoundsOrigin, ViewOrigin);
	// The screen's half height is 1 in normalized device coordinates: a radius R at depth D spans R * P / D of it.
	const float ScreenMultiple =
		FMath::Max(0.5f * FMath::Abs(ProjMatrix.M[0][0]), 0.5f * FMath::Abs(ProjMatrix.M[1][1]));
	const float ScreenRadius = ScreenMultiple * SphereRadius / FMath::Max(1.0f, Distance);
	return ScreenRadius * 2.0f;
}

int32 ComputeStaticMeshLOD(const UStaticMesh& Mesh, const FVector& BoundsOrigin, float SphereRadius,
	const FVector& ViewOrigin, const FMatrix& ProjMatrix, float DistanceScale)
{
	const int32 NumLODs = Mesh.GetNumLODs();
	if (NumLODs <= 1)
	{
		return 0;
	}
	const float ScreenSize = ComputeBoundsScreenSize(BoundsOrigin, SphereRadius, ViewOrigin, ProjMatrix) /
		FMath::Max(DistanceScale, 1.0e-3f);
	for (int32 LODIndex = NumLODs - 1; LODIndex > 0; --LODIndex)
	{
		if (Mesh.GetLODScreenSize(LODIndex) > ScreenSize)
		{
			return LODIndex;
		}
	}
	return 0;
}
