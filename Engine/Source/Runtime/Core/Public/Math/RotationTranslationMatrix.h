#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

/** Rotation from a rotator, then a translation (UE: FRotationTranslationMatrix). */
struct FRotationTranslationMatrix : public FMatrix
{
	FORCEINLINE FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin)
	{
		float SP, SY, SR;
		float CP, CY, CR;
		FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(Rot.Pitch));
		FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(Rot.Yaw));
		FMath::SinCos(&SR, &CR, FMath::DegreesToRadians(Rot.Roll));

		M[0][0] = CP * CY;
		M[0][1] = CP * SY;
		M[0][2] = SP;
		M[0][3] = 0.f;

		M[1][0] = SR * SP * CY - CR * SY;
		M[1][1] = SR * SP * SY + CR * CY;
		M[1][2] = -SR * CP;
		M[1][3] = 0.f;

		M[2][0] = -(CR * SP * CY + SR * SY);
		M[2][1] = CY * SR - CR * SP * SY;
		M[2][2] = CR * CP;
		M[2][3] = 0.f;

		M[3][0] = Origin.X;
		M[3][1] = Origin.Y;
		M[3][2] = Origin.Z;
		M[3][3] = 1.f;
	}

	static FORCEINLINE FMatrix Make(const FRotator& Rot, const FVector& Origin)
	{
		return FRotationTranslationMatrix(Rot, Origin);
	}
};
