#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/Box.h"
#include "Math/MathFwd.h"
#include "Math/Sphere.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

/** Box and sphere bounds sharing an origin (UE: FBoxSphereBounds). */
struct CORE_API FBoxSphereBounds
{
	FVector Origin;
	FVector BoxExtent;
	float SphereRadius;

	/** Uninitialised (UE). */
	FBoxSphereBounds() = default;

	explicit FORCEINLINE FBoxSphereBounds(EForceInit)
		: Origin(ForceInit)
		, BoxExtent(ForceInit)
		, SphereRadius(0.f)
	{
	}

	FORCEINLINE FBoxSphereBounds(const FVector& InOrigin, const FVector& InBoxExtent, float InSphereRadius)
		: Origin(InOrigin)
		, BoxExtent(InBoxExtent)
		, SphereRadius(InSphereRadius)
	{
	}

	/** From a box and a sphere; the radius is the smaller of the two (UE). */
	FBoxSphereBounds(const FBox& Box, const FSphere& Sphere);

	/** From a box; the sphere encloses it. */
	FORCEINLINE FBoxSphereBounds(const FBox& Box)
	{
		Box.GetCenterAndExtents(Origin, BoxExtent);
		SphereRadius = BoxExtent.Size();
	}

	FORCEINLINE FBoxSphereBounds(const FSphere& Sphere)
		: Origin(Sphere.Center)
		, BoxExtent(FVector(Sphere.W))
		, SphereRadius(Sphere.W)
	{
	}

	/** Bounds of the points (UE). */
	FBoxSphereBounds(const FVector* Points, uint32 NumPoints);

	/** Union of both bounds (UE: operator+). */
	FBoxSphereBounds operator+(const FBoxSphereBounds& Other) const;

	FORCEINLINE bool operator==(const FBoxSphereBounds& Other) const
	{
		return Origin == Other.Origin && BoxExtent == Other.BoxExtent && SphereRadius == Other.SphereRadius;
	}

	FORCEINLINE bool operator!=(const FBoxSphereBounds& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE float ComputeSquaredDistanceFromBoxToPoint(const FVector& Point) const
	{
		const FVector Mins = Origin - BoxExtent;
		const FVector Maxs = Origin + BoxExtent;
		return FBox(Mins, Maxs).ComputeSquaredDistanceToPoint(Point);
	}

	FORCEINLINE static bool SpheresIntersect(
		const FBoxSphereBounds& A, const FBoxSphereBounds& B, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return (A.Origin - B.Origin).SizeSquared() <=
			FMath::Square(FMath::Max(0.f, A.SphereRadius + B.SphereRadius + Tolerance));
	}

	FORCEINLINE static bool BoxesIntersect(const FBoxSphereBounds& A, const FBoxSphereBounds& B)
	{
		return A.GetBox().Intersect(B.GetBox());
	}

	FORCEINLINE FBox GetBox() const
	{
		return FBox(Origin - BoxExtent, Origin + BoxExtent);
	}

	FORCEINLINE FSphere GetSphere() const
	{
		return FSphere(Origin, SphereRadius);
	}

	FORCEINLINE FBoxSphereBounds ExpandBy(float ExpandAmount) const
	{
		return FBoxSphereBounds(Origin, BoxExtent + ExpandAmount, SphereRadius + ExpandAmount);
	}

	/** Bounds transformed by a matrix (UE: TransformBy). */
	FBoxSphereBounds TransformBy(const FMatrix& M) const;
	FBoxSphereBounds TransformBy(const FTransform& M) const;

	FString ToString() const;

	/** Union of two bounds (UE: Union). */
	friend FBoxSphereBounds Union(const FBoxSphereBounds& A, const FBoxSphereBounds& B)
	{
		return A + B;
	}
};

inline FArchive& operator<<(FArchive& Ar, FBoxSphereBounds& V)
{
	return Ar << V.Origin << V.BoxExtent << V.SphereRadius;
}
