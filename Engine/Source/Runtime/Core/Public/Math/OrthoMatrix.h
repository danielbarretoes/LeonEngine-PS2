#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"

/** Orthographic projection in UE clip space; Width and Height are half sizes (UE: FOrthoMatrix). */
struct FOrthoMatrix : public FMatrix
{
	FORCEINLINE FOrthoMatrix(float Width, float Height, float ZScale, float ZOffset)
		: FMatrix(FPlane((Width != 0.0f) ? (1.0f / Width) : 1.0f, 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, (Height != 0.0f) ? (1.0f / Height) : 1.f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, ZScale, 0.0f),
			  FPlane(0.0f, 0.0f, ZOffset * ZScale, 1.0f))
	{
	}
};

/** Orthographic projection with reversed depth (UE: FReversedZOrthoMatrix). */
struct FReversedZOrthoMatrix : public FMatrix
{
	FORCEINLINE FReversedZOrthoMatrix(float Width, float Height, float ZScale, float ZOffset)
		: FMatrix(FPlane((Width != 0.0f) ? (1.0f / Width) : 1.0f, 0.0f, 0.0f, 0.0f),
			  FPlane(0.0f, (Height != 0.0f) ? (1.0f / Height) : 1.f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, -ZScale, 0.0f),
			  FPlane(0.0f, 0.0f, 1.0f - ZOffset * ZScale, 1.0f))
	{
	}

	FORCEINLINE FReversedZOrthoMatrix(float Left, float Right, float Bottom, float Top, float ZScale, float ZOffset)
		: FMatrix(FPlane(1.0f / (Right - Left), 0.0f, 0.0f, 0.0f), FPlane(0.0f, 1.0f / (Top - Bottom), 0.0f, 0.0f),
			  FPlane(0.0f, 0.0f, -ZScale, 0.0f),
			  FPlane((Left + Right) / (Left - Right), (Top + Bottom) / (Bottom - Top), 1.0f - ZOffset * ZScale, 1.0f))
	{
	}
};
