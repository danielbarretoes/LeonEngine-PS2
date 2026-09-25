#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

/** A 2D point of integers (UE: FIntPoint). */
struct CORE_API FIntPoint
{
	int32 X;
	int32 Y;

	static const FIntPoint ZeroValue;
	static const FIntPoint NoneValue;

	FIntPoint() = default;

	FORCEINLINE constexpr FIntPoint(int32 InX, int32 InY)
		: X(InX)
		, Y(InY)
	{
	}

	explicit FORCEINLINE constexpr FIntPoint(int32 InXY)
		: X(InXY)
		, Y(InXY)
	{
	}

	explicit FORCEINLINE constexpr FIntPoint(EForceInit)
		: X(0)
		, Y(0)
	{
	}

	FORCEINLINE const int32& operator()(int32 PointIndex) const
	{
		return (&X)[PointIndex];
	}
	FORCEINLINE int32& operator()(int32 PointIndex)
	{
		return (&X)[PointIndex];
	}
	FORCEINLINE int32& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < 2);
		return (&X)[Index];
	}
	FORCEINLINE int32 operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < 2);
		return (&X)[Index];
	}

	FORCEINLINE bool operator==(const FIntPoint& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}
	FORCEINLINE bool operator!=(const FIntPoint& Other) const
	{
		return X != Other.X || Y != Other.Y;
	}
	FORCEINLINE FIntPoint& operator*=(int32 Scale)
	{
		X *= Scale;
		Y *= Scale;
		return *this;
	}
	FORCEINLINE FIntPoint& operator/=(int32 Divisor)
	{
		X /= Divisor;
		Y /= Divisor;
		return *this;
	}
	FORCEINLINE FIntPoint& operator+=(const FIntPoint& Other)
	{
		X += Other.X;
		Y += Other.Y;
		return *this;
	}
	FORCEINLINE FIntPoint& operator*=(const FIntPoint& Other)
	{
		X *= Other.X;
		Y *= Other.Y;
		return *this;
	}
	FORCEINLINE FIntPoint& operator-=(const FIntPoint& Other)
	{
		X -= Other.X;
		Y -= Other.Y;
		return *this;
	}
	FORCEINLINE FIntPoint& operator/=(const FIntPoint& Other)
	{
		X /= Other.X;
		Y /= Other.Y;
		return *this;
	}
	FORCEINLINE FIntPoint operator*(int32 Scale) const
	{
		return FIntPoint(*this) *= Scale;
	}
	FORCEINLINE FIntPoint operator/(int32 Divisor) const
	{
		return FIntPoint(*this) /= Divisor;
	}
	FORCEINLINE FIntPoint operator+(const FIntPoint& Other) const
	{
		return FIntPoint(*this) += Other;
	}
	FORCEINLINE FIntPoint operator*(const FIntPoint& Other) const
	{
		return FIntPoint(*this) *= Other;
	}
	FORCEINLINE FIntPoint operator-(const FIntPoint& Other) const
	{
		return FIntPoint(*this) -= Other;
	}
	FORCEINLINE FIntPoint operator/(const FIntPoint& Other) const
	{
		return FIntPoint(*this) /= Other;
	}

	FORCEINLINE FIntPoint ComponentMin(const FIntPoint& Other) const
	{
		return FIntPoint(FMath::Min(X, Other.X), FMath::Min(Y, Other.Y));
	}
	FORCEINLINE FIntPoint ComponentMax(const FIntPoint& Other) const
	{
		return FIntPoint(FMath::Max(X, Other.X), FMath::Max(Y, Other.Y));
	}
	FORCEINLINE int32 GetMax() const
	{
		return FMath::Max(X, Y);
	}
	FORCEINLINE int32 GetMin() const
	{
		return FMath::Min(X, Y);
	}
	FORCEINLINE int32 Size() const
	{
		const int64 LocalX64 = (int64)X;
		const int64 LocalY64 = (int64)Y;
		return int32(FMath::Sqrt(float(LocalX64 * LocalX64 + LocalY64 * LocalY64)));
	}
	FORCEINLINE int32 SizeSquared() const
	{
		return X * X + Y * Y;
	}

	static FORCEINLINE FIntPoint DivideAndRoundUp(FIntPoint Lhs, int32 Divisor)
	{
		return FIntPoint(FMath::DivideAndRoundUp(Lhs.X, Divisor), FMath::DivideAndRoundUp(Lhs.Y, Divisor));
	}
	static FORCEINLINE FIntPoint DivideAndRoundUp(FIntPoint Lhs, FIntPoint Divisor)
	{
		return FIntPoint(FMath::DivideAndRoundUp(Lhs.X, Divisor.X), FMath::DivideAndRoundUp(Lhs.Y, Divisor.Y));
	}
	static FORCEINLINE FIntPoint DivideAndRoundDown(FIntPoint Lhs, int32 Divisor)
	{
		return FIntPoint(FMath::DivideAndRoundDown(Lhs.X, Divisor), FMath::DivideAndRoundDown(Lhs.Y, Divisor));
	}

	static FORCEINLINE int32 Num()
	{
		return 2;
	}

	/** "X=%d Y=%d" (UE). */
	FString ToString() const
	{
		return FString::Printf("X=%d Y=%d", X, Y);
	}
	bool InitFromString(const FString& InSourceString);

	FORCEINLINE friend uint32 GetTypeHash(const FIntPoint& InPoint)
	{
		return HashCombine(GetTypeHash(InPoint.X), GetTypeHash(InPoint.Y));
	}
};
