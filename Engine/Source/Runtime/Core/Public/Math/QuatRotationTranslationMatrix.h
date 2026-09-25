#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"

/** Rotation from a quaternion, then a translation (UE: FQuatRotationTranslationMatrix). */
struct FQuatRotationTranslationMatrix : public FMatrix
{
	FORCEINLINE FQuatRotationTranslationMatrix(const FQuat& Q, const FVector& Origin)
	{
		const float X2 = Q.X + Q.X;
		const float Y2 = Q.Y + Q.Y;
		const float Z2 = Q.Z + Q.Z;
		const float XX = Q.X * X2;
		const float XY = Q.X * Y2;
		const float XZ = Q.X * Z2;
		const float YY = Q.Y * Y2;
		const float YZ = Q.Y * Z2;
		const float ZZ = Q.Z * Z2;
		const float WX = Q.W * X2;
		const float WY = Q.W * Y2;
		const float WZ = Q.W * Z2;

		M[0][0] = 1.0f - (YY + ZZ);
		M[1][0] = XY - WZ;
		M[2][0] = XZ + WY;
		M[3][0] = Origin.X;
		M[0][1] = XY + WZ;
		M[1][1] = 1.0f - (XX + ZZ);
		M[2][1] = YZ - WX;
		M[3][1] = Origin.Y;
		M[0][2] = XZ - WY;
		M[1][2] = YZ + WX;
		M[2][2] = 1.0f - (XX + YY);
		M[3][2] = Origin.Z;
		M[0][3] = 0.0f;
		M[1][3] = 0.0f;
		M[2][3] = 0.0f;
		M[3][3] = 1.0f;
	}

	static FORCEINLINE FMatrix Make(const FQuat& Q, const FVector& Origin)
	{
		return FQuatRotationTranslationMatrix(Q, Origin);
	}
};

/** Rotation matrix from a quaternion (UE: FQuatRotationMatrix). */
struct FQuatRotationMatrix : public FQuatRotationTranslationMatrix
{
	explicit FORCEINLINE FQuatRotationMatrix(const FQuat& Q)
		: FQuatRotationTranslationMatrix(Q, FVector::ZeroVector)
	{
	}

	static FORCEINLINE FMatrix Make(const FQuat& Q)
	{
		return FQuatRotationMatrix(Q);
	}
};
