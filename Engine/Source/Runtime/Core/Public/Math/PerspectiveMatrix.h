#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"

// Projection matrices in UE's clip space: row vectors, the view looks down +Z and depth is in [0, 1] (D3D style).
// The OpenGL renderer converts to its own clip space.

#define Z_PRECISION 0.0f

/** Perspective projection, depth 0 at MinZ and 1 at MaxZ (UE: FPerspectiveMatrix). */
struct FPerspectiveMatrix : public FMatrix
{
	FORCEINLINE FPerspectiveMatrix(
		float HalfFOVX, float HalfFOVY, float MultFOVX, float MultFOVY, float MinZ, float MaxZ)
		: FMatrix(FPlane(MultFOVX / FMath::Tan(HalfFOVX), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, MultFOVY / FMath::Tan(HalfFOVY), 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)), 1.0f),
			  FPlane(0.0f, 0.0f, -MinZ * ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)), 0.0f))
	{
	}

	FORCEINLINE FPerspectiveMatrix(float HalfFOV, float Width, float Height, float MinZ, float MaxZ)
		: FMatrix(FPlane(1.0f / FMath::Tan(HalfFOV), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, Width / FMath::Tan(HalfFOV) / Height, 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)), 1.0f),
			  FPlane(0.0f, 0.0f, -MinZ * ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)), 0.0f))
	{
	}

	/** Infinite far plane. */
	FORCEINLINE FPerspectiveMatrix(float HalfFOV, float Width, float Height, float MinZ)
		: FMatrix(FPlane(1.0f / FMath::Tan(HalfFOV), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, Width / FMath::Tan(HalfFOV) / Height, 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, (1.0f - Z_PRECISION), 1.0f), FPlane(0.0f, 0.0f, -MinZ * (1.0f - Z_PRECISION), 0.0f))
	{
	}
};

/** Perspective projection with depth 1 at MinZ and 0 at MaxZ (UE: FReversedZPerspectiveMatrix). */
struct FReversedZPerspectiveMatrix : public FMatrix
{
	FORCEINLINE FReversedZPerspectiveMatrix(
		float HalfFOVX, float HalfFOVY, float MultFOVX, float MultFOVY, float MinZ, float MaxZ)
		: FMatrix(FPlane(MultFOVX / FMath::Tan(HalfFOVX), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, MultFOVY / FMath::Tan(HalfFOVY), 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? 0.0f : MinZ / (MinZ - MaxZ)), 1.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? MinZ : -MaxZ * MinZ / (MinZ - MaxZ)), 0.0f))
	{
	}

	FORCEINLINE FReversedZPerspectiveMatrix(float HalfFOV, float Width, float Height, float MinZ, float MaxZ)
		: FMatrix(FPlane(1.0f / FMath::Tan(HalfFOV), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, Width / FMath::Tan(HalfFOV) / Height, 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? 0.0f : MinZ / (MinZ - MaxZ)), 1.0f),
			  FPlane(0.0f, 0.0f, ((MinZ == MaxZ) ? MinZ : -MaxZ * MinZ / (MinZ - MaxZ)), 0.0f))
	{
	}

	/** Infinite far plane. */
	FORCEINLINE FReversedZPerspectiveMatrix(float HalfFOV, float Width, float Height, float MinZ)
		: FMatrix(FPlane(1.0f / FMath::Tan(HalfFOV), 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, Width / FMath::Tan(HalfFOV) / Height, 0.0f, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f),
			  FPlane(0.0f, 0.0f, MinZ, 0.0f))
	{
	}
};
