#pragma once

#include "CoreTypes.h"
#include "Math/RotationTranslationMatrix.h"

/** Rotation about Origin instead of the world origin (UE: FRotationAboutPointMatrix). */
struct FRotationAboutPointMatrix : public FRotationTranslationMatrix
{
	FORCEINLINE FRotationAboutPointMatrix(const FRotator& Rot, const FVector& Origin)
		: FRotationTranslationMatrix(Rot, Origin)
	{
		// FRotationTranslationMatrix generates R * T. We need -T * R * T, so prepend that translation.
		const FVector XAxis(M[0][0], M[1][0], M[2][0]);
		const FVector YAxis(M[0][1], M[1][1], M[2][1]);
		const FVector ZAxis(M[0][2], M[1][2], M[2][2]);

		M[3][0] -= XAxis | Origin;
		M[3][1] -= YAxis | Origin;
		M[3][2] -= ZAxis | Origin;
	}

	static FORCEINLINE FMatrix Make(const FRotator& Rot, const FVector& Origin)
	{
		return FRotationAboutPointMatrix(Rot, Origin);
	}
};
