#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

/** A 3D vector of integers (UE: FIntVector). */
struct CORE_API FIntVector
{
	int32 X;
	int32 Y;
	int32 Z;

	static const FIntVector ZeroValue;
	static const FIntVector NoneValue;

	FIntVector() = default;

	FORCEINLINE constexpr FIntVector(int32 InX, int32 InY, int32 InZ)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}

	explicit FORCEINLINE constexpr FIntVector(int32 InValue)
		: X(InValue)
		, Y(InValue)
		, Z(InValue)
	{
	}

	explicit FORCEINLINE constexpr FIntVector(EForceInit)
		: X(0)
		, Y(0)
		, Z(0)
	{
	}

	/** Truncates a float vector (UE: FIntVector(FVector)). */
	explicit FIntVector(const struct FVector& InVector);

	FORCEINLINE const int32& operator()(int32 ComponentIndex) const
	{
		return (&X)[ComponentIndex];
	}
	FORCEINLINE int32& operator()(int32 ComponentIndex)
	{
		return (&X)[ComponentIndex];
	}
	FORCEINLINE int32& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < 3);
		return (&X)[Index];
	}
	FORCEINLINE int32 operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < 3);
		return (&X)[Index];
	}

	FORCEINLINE bool operator==(const FIntVector& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}
	FORCEINLINE bool operator!=(const FIntVector& Other) const
	{
		return X != Other.X || Y != Other.Y || Z != Other.Z;
	}
	FORCEINLINE FIntVector& operator*=(int32 Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		return *this;
	}
	FORCEINLINE FIntVector& operator/=(int32 Divisor)
	{
		X /= Divisor;
		Y /= Divisor;
		Z /= Divisor;
		return *this;
	}
	FORCEINLINE FIntVector& operator+=(const FIntVector& Other)
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		return *this;
	}
	FORCEINLINE FIntVector& operator-=(const FIntVector& Other)
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		return *this;
	}
	FORCEINLINE FIntVector operator*(int32 Scale) const
	{
		return FIntVector(*this) *= Scale;
	}
	FORCEINLINE FIntVector operator/(int32 Divisor) const
	{
		return FIntVector(*this) /= Divisor;
	}
	FORCEINLINE FIntVector operator+(const FIntVector& Other) const
	{
		return FIntVector(*this) += Other;
	}
	FORCEINLINE FIntVector operator-(const FIntVector& Other) const
	{
		return FIntVector(*this) -= Other;
	}

	FORCEINLINE int32 GetMax() const
	{
		return FMath::Max(FMath::Max(X, Y), Z);
	}
	FORCEINLINE int32 GetMin() const
	{
		return FMath::Min(FMath::Min(X, Y), Z);
	}
	FORCEINLINE int32 Size() const
	{
		const int64 LocalX64 = (int64)X;
		const int64 LocalY64 = (int64)Y;
		const int64 LocalZ64 = (int64)Z;
		return int32(FMath::Sqrt(float(LocalX64 * LocalX64 + LocalY64 * LocalY64 + LocalZ64 * LocalZ64)));
	}
	FORCEINLINE bool IsZero() const
	{
		return *this == ZeroValue;
	}

	static FORCEINLINE FIntVector DivideAndRoundUp(FIntVector Lhs, int32 Divisor)
	{
		return FIntVector(FMath::DivideAndRoundUp(Lhs.X, Divisor), FMath::DivideAndRoundUp(Lhs.Y, Divisor),
			FMath::DivideAndRoundUp(Lhs.Z, Divisor));
	}

	static FORCEINLINE int32 Num()
	{
		return 3;
	}

	/** "X=%d Y=%d Z=%d" (UE). */
	FString ToString() const
	{
		return FString::Printf("X=%d Y=%d Z=%d", X, Y, Z);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FIntVector& Vector)
	{
		return HashCombine(HashCombine(GetTypeHash(Vector.X), GetTypeHash(Vector.Y)), GetTypeHash(Vector.Z));
	}
};
