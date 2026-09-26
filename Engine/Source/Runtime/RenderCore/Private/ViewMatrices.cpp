#include "ViewMatrices.h"

FMatrix MakeViewMatrix(const FVector& Origin, const FVector& Forward, const FVector& Right, const FVector& Up)
{
	// Columns are the view axes; the translation row is -Origin projected on them.
	return FMatrix(FPlane(Right.X, Up.X, Forward.X, 0.0f), FPlane(Right.Y, Up.Y, Forward.Y, 0.0f),
		FPlane(Right.Z, Up.Z, Forward.Z, 0.0f), FPlane(-(Right | Origin), -(Up | Origin), -(Forward | Origin), 1.0f));
}

FMatrix MakeViewMatrix(const FVector& Origin, const FRotator& Rotation)
{
	// UE's view axes: world forward (X after the inverse rotation) becomes view z, right (Y) view x, up (Z) view y.
	const FMatrix Swizzle(FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f));
	return FTranslationMatrix(-Origin) * FInverseRotationMatrix(Rotation) * Swizzle;
}

FMatrix MakeLookAtView(const FVector& Eye, const FVector& Target, const FVector& WorldUp)
{
	const FVector Forward = (Target - Eye).GetUnsafeNormal();
	const FVector Right = (WorldUp ^ Forward).GetUnsafeNormal();
	const FVector Up = Forward ^ Right;
	return MakeViewMatrix(Eye, Forward, Right, Up);
}
