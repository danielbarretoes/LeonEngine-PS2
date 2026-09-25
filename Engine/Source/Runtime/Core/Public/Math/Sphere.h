#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

/** Bounding sphere (UE: FSphere). W is the radius; W == 0 means empty. */
struct CORE_API FSphere
{
	FVector Center;
	float W;

	/** Uninitialised (UE). */
	FSphere() = default;

	explicit FORCEINLINE FSphere(EForceInit)
		: Center(ForceInit)
		, W(0.0f)
	{
	}

	FORCEINLINE FSphere(FVector InV, float InW)
		: Center(InV)
		, W(InW)
	{
	}

	/** Bounding sphere of the points (UE: FSphere(const FVector*, int32)). */
	FSphere(const FVector* Pts, int32 Count);

	FORCEINLINE bool Equals(const FSphere& Sphere, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return Center.Equals(Sphere.Center, Tolerance) && FMath::Abs(W - Sphere.W) <= Tolerance;
	}

	/** Other is entirely inside this sphere (UE: IsInside). */
	FORCEINLINE bool IsInside(const FSphere& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		if (W < Other.W - Tolerance)
		{
			return false;
		}
		return (Center - Other.Center).SizeSquared() <= FMath::Square(W - Other.W + Tolerance);
	}

	FORCEINLINE bool IsInside(const FVector& In, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return (Center - In).SizeSquared() <= FMath::Square(W + Tolerance);
	}

	FORCEINLINE bool Intersects(const FSphere& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return (Center - Other.Center).SizeSquared() <= FMath::Square(FMath::Max(0.f, Other.W + W + Tolerance));
	}

	/** Sphere transformed by a matrix; the radius uses the largest axis scale (UE: TransformBy). */
	FSphere TransformBy(const FMatrix& M) const;
	FSphere TransformBy(const FTransform& M) const;

	float GetVolume() const;

	/** Smallest sphere containing both (UE: operator+=). */
	FSphere& operator+=(const FSphere& Other);

	FORCEINLINE FSphere operator+(const FSphere& Other) const
	{
		return FSphere(*this) += Other;
	}
};
