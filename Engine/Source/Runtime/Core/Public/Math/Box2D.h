#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Serialization/Archive.h"

/** Axis-aligned 2D box; bIsValid is false for an empty box (UE: FBox2D). */
struct CORE_API FBox2D
{
	FVector2D Min;
	FVector2D Max;
	bool bIsValid;

	/** Uninitialised (UE). */
	FBox2D() = default;

	explicit FORCEINLINE FBox2D(EForceInit)
	{
		Init();
	}

	FORCEINLINE FBox2D(const FVector2D& InMin, const FVector2D& InMax)
		: Min(InMin)
		, Max(InMax)
		, bIsValid(true)
	{
	}

	FORCEINLINE bool operator==(const FBox2D& Other) const
	{
		return (Min == Other.Min) && (Max == Other.Max);
	}

	FORCEINLINE FBox2D& operator+=(const FVector2D& Other)
	{
		if (bIsValid)
		{
			Min.X = FMath::Min(Min.X, Other.X);
			Min.Y = FMath::Min(Min.Y, Other.Y);
			Max.X = FMath::Max(Max.X, Other.X);
			Max.Y = FMath::Max(Max.Y, Other.Y);
		}
		else
		{
			Min = Max = Other;
			bIsValid = true;
		}
		return *this;
	}

	FORCEINLINE FBox2D& operator+=(const FBox2D& Other)
	{
		if (bIsValid && Other.bIsValid)
		{
			Min.X = FMath::Min(Min.X, Other.Min.X);
			Min.Y = FMath::Min(Min.Y, Other.Min.Y);
			Max.X = FMath::Max(Max.X, Other.Max.X);
			Max.Y = FMath::Max(Max.Y, Other.Max.Y);
		}
		else if (Other.bIsValid)
		{
			*this = Other;
		}
		return *this;
	}

	FORCEINLINE void Init()
	{
		Min = Max = FVector2D::ZeroVector;
		bIsValid = false;
	}

	FORCEINLINE FVector2D GetCenter() const
	{
		return FVector2D((Min + Max) * 0.5f);
	}

	FORCEINLINE FVector2D GetExtent() const
	{
		return 0.5f * (Max - Min);
	}

	FORCEINLINE FVector2D GetSize() const
	{
		return (Max - Min);
	}

	FORCEINLINE float GetArea() const
	{
		return (Max.X - Min.X) * (Max.Y - Min.Y);
	}

	FORCEINLINE bool Intersect(const FBox2D& Other) const
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

	/** Strictly inside (UE: IsInside). */
	FORCEINLINE bool IsInside(const FVector2D& TestPoint) const
	{
		return ((TestPoint.X > Min.X) && (TestPoint.X < Max.X) && (TestPoint.Y > Min.Y) && (TestPoint.Y < Max.Y));
	}

	FORCEINLINE FBox2D ExpandBy(float W) const
	{
		return FBox2D(Min - FVector2D(W, W), Max + FVector2D(W, W));
	}

	FORCEINLINE FBox2D ShiftBy(const FVector2D& Offset) const
	{
		return FBox2D(Min + Offset, Max + Offset);
	}

	FString ToString() const
	{
		return FString::Printf(
			"bIsValid=%s, Min=(%s), Max=(%s)", bIsValid ? "true" : "false", *Min.ToString(), *Max.ToString());
	}
};

inline FArchive& operator<<(FArchive& Ar, FBox2D& V)
{
	return Ar << V.Min << V.Max << V.bIsValid;
}
