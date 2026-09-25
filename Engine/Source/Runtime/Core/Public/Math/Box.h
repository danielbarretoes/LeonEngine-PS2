#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

/** Axis-aligned bounding box; IsValid is 0 for an empty box (UE: FBox). */
struct CORE_API FBox
{
	FVector Min;
	FVector Max;
	uint8 IsValid;

	/** Uninitialised (UE). */
	FBox() = default;

	/** Empty box (UE: FBox(EForceInit)). */
	explicit FORCEINLINE FBox(EForceInit)
	{
		Init();
	}

	FORCEINLINE FBox(const FVector& InMin, const FVector& InMax)
		: Min(InMin)
		, Max(InMax)
		, IsValid(1)
	{
	}

	/** Smallest box containing the points. */
	FBox(const FVector* Points, int32 Count);

	FORCEINLINE bool operator==(const FBox& Other) const
	{
		return (Min == Other.Min) && (Max == Other.Max);
	}

	FORCEINLINE bool operator!=(const FBox& Other) const
	{
		return !(*this == Other);
	}

	/** Grows the box to contain Other (UE: operator+=). */
	FORCEINLINE FBox& operator+=(const FVector& Other)
	{
		if (IsValid)
		{
			Min.X = FMath::Min(Min.X, Other.X);
			Min.Y = FMath::Min(Min.Y, Other.Y);
			Min.Z = FMath::Min(Min.Z, Other.Z);
			Max.X = FMath::Max(Max.X, Other.X);
			Max.Y = FMath::Max(Max.Y, Other.Y);
			Max.Z = FMath::Max(Max.Z, Other.Z);
		}
		else
		{
			Min = Max = Other;
			IsValid = 1;
		}
		return *this;
	}

	FORCEINLINE FBox operator+(const FVector& Other) const
	{
		return FBox(*this) += Other;
	}

	FBox& operator+=(const FBox& Other);

	FORCEINLINE FBox operator+(const FBox& Other) const
	{
		return FBox(*this) += Other;
	}

	/** Min (0) or Max (1). */
	FORCEINLINE FVector& operator[](int32 Index)
	{
		checkSlow((Index >= 0) && (Index < 2));
		return Index == 0 ? Min : Max;
	}

	FORCEINLINE float ComputeSquaredDistanceToPoint(const FVector& Point) const
	{
		float DistSquared = 0.f;
		if (Point.X < Min.X)
		{
			DistSquared += FMath::Square(Point.X - Min.X);
		}
		else if (Point.X > Max.X)
		{
			DistSquared += FMath::Square(Point.X - Max.X);
		}
		if (Point.Y < Min.Y)
		{
			DistSquared += FMath::Square(Point.Y - Min.Y);
		}
		else if (Point.Y > Max.Y)
		{
			DistSquared += FMath::Square(Point.Y - Max.Y);
		}
		if (Point.Z < Min.Z)
		{
			DistSquared += FMath::Square(Point.Z - Min.Z);
		}
		else if (Point.Z > Max.Z)
		{
			DistSquared += FMath::Square(Point.Z - Max.Z);
		}
		return DistSquared;
	}

	/** Grown by W on every side (UE: ExpandBy). */
	FORCEINLINE FBox ExpandBy(float W) const
	{
		return FBox(Min - FVector(W, W, W), Max + FVector(W, W, W));
	}
	FORCEINLINE FBox ExpandBy(const FVector& V) const
	{
		return FBox(Min - V, Max + V);
	}
	FORCEINLINE FBox ExpandBy(const FVector& Neg, const FVector& Pos) const
	{
		return FBox(Min - Neg, Max + Pos);
	}

	FORCEINLINE FBox ShiftBy(const FVector& Offset) const
	{
		return FBox(Min + Offset, Max + Offset);
	}

	FORCEINLINE FBox MoveTo(const FVector& Destination) const
	{
		const FVector Offset = Destination - GetCenter();
		return FBox(Min + Offset, Max + Offset);
	}

	FORCEINLINE FVector GetCenter() const
	{
		return FVector((Min + Max) * 0.5f);
	}

	FORCEINLINE void GetCenterAndExtents(FVector& Center, FVector& Extents) const
	{
		Extents = GetExtent();
		Center = Min + Extents;
	}

	/** Point of the box closest to Point (UE: GetClosestPointTo). */
	FVector GetClosestPointTo(const FVector& Point) const;

	/** Half the size (UE: GetExtent). */
	FORCEINLINE FVector GetExtent() const
	{
		return 0.5f * (Max - Min);
	}

	FORCEINLINE FVector GetSize() const
	{
		return (Max - Min);
	}

	FORCEINLINE float GetVolume() const
	{
		return ((Max.X - Min.X) * (Max.Y - Min.Y) * (Max.Z - Min.Z));
	}

	/** Resets to an empty box (UE: Init). */
	FORCEINLINE void Init()
	{
		Min = Max = FVector::ZeroVector;
		IsValid = 0;
	}

	/** Boxes overlap, touching included (UE: Intersect). */
	FORCEINLINE bool Intersect(const FBox& Other) const
	{
		if ((Min.X > Other.Max.X) || (Other.Min.X > Max.X))
		{
			return false;
		}
		if ((Min.Y > Other.Max.Y) || (Other.Min.Y > Max.Y))
		{
			return false;
		}
		if ((Min.Z > Other.Max.Z) || (Other.Min.Z > Max.Z))
		{
			return false;
		}
		return true;
	}

	FORCEINLINE bool IntersectXY(const FBox& Other) const
	{
		if ((Min.X > Other.Max.X) || (Other.Min.X > Max.X))
		{
			return false;
		}
		if ((Min.Y > Other.Max.Y) || (Other.Min.Y > Max.Y))
		{
			return false;
		}
		return true;
	}

	/** The overlap of both boxes (UE: Overlap). */
	FBox Overlap(const FBox& Other) const;

	/** Strictly inside (UE: IsInside). */
	FORCEINLINE bool IsInside(const FVector& In) const
	{
		return (
			(In.X > Min.X) && (In.X < Max.X) && (In.Y > Min.Y) && (In.Y < Max.Y) && (In.Z > Min.Z) && (In.Z < Max.Z));
	}

	FORCEINLINE bool IsInsideOrOn(const FVector& In) const
	{
		return ((In.X >= Min.X) && (In.X <= Max.X) && (In.Y >= Min.Y) && (In.Y <= Max.Y) && (In.Z >= Min.Z) &&
			(In.Z <= Max.Z));
	}

	FORCEINLINE bool IsInside(const FBox& Other) const
	{
		return (IsInside(Other.Min) && IsInside(Other.Max));
	}

	FORCEINLINE bool IsInsideXY(const FVector& In) const
	{
		return ((In.X > Min.X) && (In.X < Max.X) && (In.Y > Min.Y) && (In.Y < Max.Y));
	}

	/** Axis-aligned bounds of the box transformed by M (UE: TransformBy). */
	FBox TransformBy(const FMatrix& M) const;
	FBox TransformBy(const FTransform& M) const;

	/** Bounds of the box transformed by the inverse of M (UE: InverseTransformBy). */
	FBox InverseTransformBy(const FTransform& M) const;

	/** Box from a center and a half size (UE: BuildAABB). */
	static FORCEINLINE FBox BuildAABB(const FVector& Origin, const FVector& Extent)
	{
		return FBox(Origin - Extent, Origin + Extent);
	}

	FString ToString() const;
};

FORCEINLINE uint32 GetTypeHash(const FBox& Box)
{
	return HashCombine(GetTypeHash(Box.Min), GetTypeHash(Box.Max));
}

inline FArchive& operator<<(FArchive& Ar, FBox& V)
{
	return Ar << V.Min << V.Max << V.IsValid;
}
