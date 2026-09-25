#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

/** Translation only (UE: FTranslationMatrix). */
struct FTranslationMatrix : public FMatrix
{
	explicit FORCEINLINE FTranslationMatrix(const FVector& Delta)
		: FMatrix(FPlane(1.0f, 0.0f, 0.0f, 0.0f), FPlane(0.0f, 1.0f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, 1.0f, 0.0f),
			  FPlane(Delta.X, Delta.Y, Delta.Z, 1.0f))
	{
	}

	static FORCEINLINE FMatrix Make(const FVector& Delta)
	{
		return FTranslationMatrix(Delta);
	}
};
