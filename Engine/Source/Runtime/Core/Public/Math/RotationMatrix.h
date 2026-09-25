#pragma once

#include "CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/QuatRotationTranslationMatrix.h"
#include "Math/RotationTranslationMatrix.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

/** Rotation matrix from a rotator (UE: FRotationMatrix). */
struct CORE_API FRotationMatrix : public FRotationTranslationMatrix
{
	explicit FORCEINLINE FRotationMatrix(const FRotator& Rot)
		: FRotationTranslationMatrix(Rot, FVector::ZeroVector)
	{
	}

	static FORCEINLINE FMatrix Make(const FRotator& Rot)
	{
		return FRotationMatrix(Rot);
	}

	static FORCEINLINE FMatrix Make(const FQuat& Rot)
	{
		return FQuatRotationTranslationMatrix(Rot, FVector::ZeroVector);
	}

	/** Rotation whose X axis is XAxis; the others use world Z as up where possible (UE: MakeFromX). */
	static FMatrix MakeFromX(const FVector& XAxis);
	static FMatrix MakeFromY(const FVector& YAxis);
	static FMatrix MakeFromZ(const FVector& ZAxis);

	/** Rotation with an exact first axis and the second as close as possible (UE: MakeFromXY and siblings). */
	static FMatrix MakeFromXY(const FVector& XAxis, const FVector& YAxis);
	static FMatrix MakeFromXZ(const FVector& XAxis, const FVector& ZAxis);
	static FMatrix MakeFromYX(const FVector& YAxis, const FVector& XAxis);
	static FMatrix MakeFromYZ(const FVector& YAxis, const FVector& ZAxis);
	static FMatrix MakeFromZX(const FVector& ZAxis, const FVector& XAxis);
	static FMatrix MakeFromZY(const FVector& ZAxis, const FVector& YAxis);
};
