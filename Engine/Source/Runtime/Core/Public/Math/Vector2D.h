#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

/** A 2D vector of floats (UE: FVector2D). `|` is the dot product and `^` the 2D cross product (a scalar). */
struct CORE_API FVector2D
{
	float X;
	float Y;

	static const FVector2D ZeroVector;
	static const FVector2D UnitVector;
	static const FVector2D Unit45Deg;

	/** Uninitialised (UE). */
	FVector2D() = default;

	FORCEINLINE constexpr FVector2D(float InX, float InY)
		: X(InX)
		, Y(InY)
	{
	}

	explicit FORCEINLINE constexpr FVector2D(float InF)
		: X(InF)
		, Y(InF)
	{
	}

	explicit FORCEINLINE constexpr FVector2D(EForceInit)
		: X(0)
		, Y(0)
	{
	}

	FVector2D(FIntPoint InPos);

	/** Drops Z. */
	explicit FVector2D(const FVector& V);

	FORCEINLINE FVector2D operator+(const FVector2D& V) const
	{
		return FVector2D(X + V.X, Y + V.Y);
	}
	FORCEINLINE FVector2D operator-(const FVector2D& V) const
	{
		return FVector2D(X - V.X, Y - V.Y);
	}
	FORCEINLINE FVector2D operator*(float Scale) const
	{
		return FVector2D(X * Scale, Y * Scale);
	}
	FORCEINLINE FVector2D operator/(float Scale) const
	{
		const float RScale = 1.f / Scale;
		return FVector2D(X * RScale, Y * RScale);
	}
	FORCEINLINE FVector2D operator+(float A) const
	{
		return FVector2D(X + A, Y + A);
	}
	FORCEINLINE FVector2D operator-(float A) const
	{
		return FVector2D(X - A, Y - A);
	}
	FORCEINLINE FVector2D operator*(const FVector2D& V) const
	{
		return FVector2D(X * V.X, Y * V.Y);
	}
	FORCEINLINE FVector2D operator/(const FVector2D& V) const
	{
		return FVector2D(X / V.X, Y / V.Y);
	}
	/** Dot product. */
	FORCEINLINE float operator|(const FVector2D& V) const
	{
		return X * V.X + Y * V.Y;
	}
	/** 2D cross product (Z of the 3D cross product). */
	FORCEINLINE float operator^(const FVector2D& V) const
	{
		return X * V.Y - Y * V.X;
	}
	FORCEINLINE bool operator==(const FVector2D& V) const
	{
		return X == V.X && Y == V.Y;
	}
	FORCEINLINE bool operator!=(const FVector2D& V) const
	{
		return X != V.X || Y != V.Y;
	}
	FORCEINLINE bool operator<(const FVector2D& Other) const
	{
		return X < Other.X && Y < Other.Y;
	}
	FORCEINLINE bool operator>(const FVector2D& Other) const
	{
		return X > Other.X && Y > Other.Y;
	}
	FORCEINLINE bool operator<=(const FVector2D& Other) const
	{
		return X <= Other.X && Y <= Other.Y;
	}
	FORCEINLINE bool operator>=(const FVector2D& Other) const
	{
		return X >= Other.X && Y >= Other.Y;
	}
	FORCEINLINE FVector2D operator-() const
	{
		return FVector2D(-X, -Y);
	}
	FORCEINLINE FVector2D operator+=(const FVector2D& V)
	{
		X += V.X;
		Y += V.Y;
		return *this;
	}
	FORCEINLINE FVector2D operator-=(const FVector2D& V)
	{
		X -= V.X;
		Y -= V.Y;
		return *this;
	}
	FORCEINLINE FVector2D operator*=(float Scale)
	{
		X *= Scale;
		Y *= Scale;
		return *this;
	}
	FORCEINLINE FVector2D operator/=(float V)
	{
		const float RV = 1.f / V;
		X *= RV;
		Y *= RV;
		return *this;
	}
	FORCEINLINE FVector2D operator*=(const FVector2D& V)
	{
		X *= V.X;
		Y *= V.Y;
		return *this;
	}
	FORCEINLINE FVector2D operator/=(const FVector2D& V)
	{
		X /= V.X;
		Y /= V.Y;
		return *this;
	}
	FORCEINLINE float& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < 2);
		return (&X)[Index];
	}
	FORCEINLINE float operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < 2);
		return (&X)[Index];
	}
	FORCEINLINE float& Component(int32 Index)
	{
		return (&X)[Index];
	}
	FORCEINLINE float Component(int32 Index) const
	{
		return (&X)[Index];
	}

	static FORCEINLINE float DotProduct(const FVector2D& A, const FVector2D& B)
	{
		return A | B;
	}
	static FORCEINLINE float DistSquared(const FVector2D& V1, const FVector2D& V2)
	{
		return FMath::Square(V2.X - V1.X) + FMath::Square(V2.Y - V1.Y);
	}
	static FORCEINLINE float Distance(const FVector2D& V1, const FVector2D& V2)
	{
		return FMath::Sqrt(DistSquared(V1, V2));
	}
	static FORCEINLINE float CrossProduct(const FVector2D& A, const FVector2D& B)
	{
		return A ^ B;
	}

	FORCEINLINE bool Equals(const FVector2D& V, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - V.X) <= Tolerance && FMath::Abs(Y - V.Y) <= Tolerance;
	}

	FORCEINLINE void Set(float InX, float InY)
	{
		X = InX;
		Y = InY;
	}

	FORCEINLINE float GetMax() const
	{
		return FMath::Max(X, Y);
	}
	FORCEINLINE float GetAbsMax() const
	{
		return FMath::Max(FMath::Abs(X), FMath::Abs(Y));
	}
	FORCEINLINE float GetMin() const
	{
		return FMath::Min(X, Y);
	}
	FORCEINLINE float Size() const
	{
		return FMath::Sqrt(X * X + Y * Y);
	}
	FORCEINLINE float SizeSquared() const
	{
		return X * X + Y * Y;
	}

	/** Rotated by AngleDeg counter-clockwise (UE: GetRotated). */
	FVector2D GetRotated(float AngleDeg) const;

	FORCEINLINE FVector2D GetSafeNormal(float Tolerance = SMALL_NUMBER) const
	{
		const float SquareSum = X * X + Y * Y;
		if (SquareSum > Tolerance)
		{
			const float Scale = FMath::InvSqrt(SquareSum);
			return FVector2D(X * Scale, Y * Scale);
		}
		return FVector2D(0.f, 0.f);
	}

	FORCEINLINE void Normalize(float Tolerance = SMALL_NUMBER)
	{
		const float SquareSum = X * X + Y * Y;
		if (SquareSum > Tolerance)
		{
			const float Scale = FMath::InvSqrt(SquareSum);
			X *= Scale;
			Y *= Scale;
			return;
		}
		X = 0.0f;
		Y = 0.0f;
	}

	FORCEINLINE bool IsNearlyZero(float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X) <= Tolerance && FMath::Abs(Y) <= Tolerance;
	}
	FORCEINLINE bool IsZero() const
	{
		return X == 0.f && Y == 0.f;
	}

	FORCEINLINE FVector2D RoundToVector() const
	{
		return FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y));
	}

	FORCEINLINE FVector2D ClampAxes(float MinAxisVal, float MaxAxisVal) const
	{
		return FVector2D(FMath::Clamp(X, MinAxisVal, MaxAxisVal), FMath::Clamp(Y, MinAxisVal, MaxAxisVal));
	}

	FORCEINLINE FVector2D GetSignVector() const
	{
		return FVector2D(FMath::FloatSelect(X, 1.f, -1.f), FMath::FloatSelect(Y, 1.f, -1.f));
	}

	FORCEINLINE FVector2D GetAbs() const
	{
		return FVector2D(FMath::Abs(X), FMath::Abs(Y));
	}

	FORCEINLINE FVector2D ComponentMin(const FVector2D& Other) const
	{
		return FVector2D(FMath::Min(X, Other.X), FMath::Min(Y, Other.Y));
	}
	FORCEINLINE FVector2D ComponentMax(const FVector2D& Other) const
	{
		return FVector2D(FMath::Max(X, Other.X), FMath::Max(Y, Other.Y));
	}

	FORCEINLINE bool ContainsNaN() const
	{
		return !FMath::IsFinite(X) || !FMath::IsFinite(Y);
	}

	/** "X=%3.3f Y=%3.3f" (UE). */
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);
};

FORCEINLINE FVector2D operator*(float Scale, const FVector2D& V)
{
	return V.operator*(Scale);
}

FORCEINLINE uint32 GetTypeHash(const FVector2D& Vector)
{
	return HashCombine(GetTypeHash(Vector.X), GetTypeHash(Vector.Y));
}

inline FString LexToString(const FVector2D& Vector)
{
	return Vector.ToString();
}
