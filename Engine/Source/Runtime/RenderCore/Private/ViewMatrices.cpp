#include "ViewMatrices.h"

#include "GLClipSpace.h"

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

FMatrix MakeReflectMatrix(float PlaneZ)
{
	// Row vectors: z' = 2 PlaneZ - z.
	FMatrix ReflectMat = FMatrix::Identity;
	ReflectMat.M[2][2] = -1.0f;
	ReflectMat.M[3][2] = 2.0f * PlaneZ;
	return ReflectMat;
}

FMatrix FitLightSpaceMatrix(
	const FVector& LightDirection, const FVector& WorldMin, const FVector& WorldMax, float Padding)
{
	FVector Dir = LightDirection;
	if ((Dir | Dir) < 1e-8f)
	{
		Dir = FVector(0.35f, -0.45f, -1.0f);
	}
	Dir = Dir.GetUnsafeNormal();

	const FVector Center = (WorldMin + WorldMax) * 0.5f;
	const FVector Extents = (WorldMax - WorldMin) * 0.5f + FVector(Padding);
	const float Radius = Extents.Size();

	FVector Up(0.0f, 0.0f, 1.0f);
	if (FMath::Abs(Dir | Up) > 0.95f)
	{
		Up = FVector(0.0f, 1.0f, 0.0f);
	}

	/** cm between the bounding sphere and the light eye. */
	constexpr float EyeMargin = 100.0f;
	const FVector Eye = Center - (Dir * (Radius + EyeMargin));
	// UE view space of the light: x right, y up, z along the light (left-handed).
	const FMatrix LightView = MakeLookAtView(Eye, Center, Up);

	FVector MinLs(TNumericLimits<float>::Max());
	FVector MaxLs(TNumericLimits<float>::Lowest());

	const FVector Corners[8] = {
		FVector(WorldMin.X, WorldMin.Y, WorldMin.Z),
		FVector(WorldMax.X, WorldMin.Y, WorldMin.Z),
		FVector(WorldMin.X, WorldMax.Y, WorldMin.Z),
		FVector(WorldMax.X, WorldMax.Y, WorldMin.Z),
		FVector(WorldMin.X, WorldMin.Y, WorldMax.Z),
		FVector(WorldMax.X, WorldMin.Y, WorldMax.Z),
		FVector(WorldMin.X, WorldMax.Y, WorldMax.Z),
		FVector(WorldMax.X, WorldMax.Y, WorldMax.Z),
	};

	for (const FVector& Corner : Corners)
	{
		const FVector Ls = FVector(LightView.TransformPosition(Corner));
		MinLs = MinLs.ComponentMin(Ls);
		MaxLs = MaxLs.ComponentMax(Ls);
	}

	// View-space depth grows along +Z in front of the light (cm).
	const float ZNear = FMath::Max(5.0f, MinLs.Z - Padding);
	const float ZFar = FMath::Max(ZNear + 10.0f, MaxLs.Z + Padding);

	// Off-centre box: centre it in x / y, then a UE ortho with half sizes (depth [0, 1] from ZNear to ZFar), then GL
	// clip space.
	const float Left = MinLs.X - Padding;
	const float Right = MaxLs.X + Padding;
	const float Bottom = MinLs.Y - Padding;
	const float Top = MaxLs.Y + Padding;
	const FMatrix Centre = FTranslationMatrix(FVector(-(Left + Right) * 0.5f, -(Bottom + Top) * 0.5f, 0.0f));
	const FMatrix LightProj =
		Centre * FOrthoMatrix((Right - Left) * 0.5f, (Top - Bottom) * 0.5f, 1.0f / (ZFar - ZNear), -ZNear);
	return LightView * ToGLClipSpace(LightProj);
}
