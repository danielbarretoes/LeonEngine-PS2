#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

/** Scale only (UE: FScaleMatrix). */
struct FScaleMatrix : public FMatrix
{
	explicit FORCEINLINE FScaleMatrix(float Scale)
		: FMatrix(FPlane(Scale, 0.0f, 0.0f, 0.0f), FPlane(0.0f, Scale, 0.0f, 0.0f), FPlane(0.0f, 0.0f, Scale, 0.0f),
			  FPlane(0.0f, 0.0f, 0.0f, 1.0f))
	{
	}

	explicit FORCEINLINE FScaleMatrix(const FVector& Scale)
		: FMatrix(FPlane(Scale.X, 0.0f, 0.0f, 0.0f), FPlane(0.0f, Scale.Y, 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, Scale.Z, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f))
	{
	}

	static FORCEINLINE FMatrix Make(float Scale)
	{
		return FScaleMatrix(Scale);
	}

	static FORCEINLINE FMatrix Make(const FVector& Scale)
	{
		return FScaleMatrix(Scale);
	}
};
