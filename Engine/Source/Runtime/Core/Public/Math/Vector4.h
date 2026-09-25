#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

/** A 4D homogeneous vector of floats (UE: FVector4). */
struct CORE_API FVector4
{
	float X;
	float Y;
	float Z;
	float W;

	FORCEINLINE FVector4(const FVector& InVector, float InW = 1.0f)
		: X(InVector.X)
		, Y(InVector.Y)
		, Z(InVector.Z)
		, W(InW)
	{
	}

	explicit FVector4(const FLinearColor& InColor);

	/** (0, 0, 0, 1) by default like UE's FVector4(float = 0...) with W = 1. */
	explicit FORCEINLINE constexpr FVector4(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f, float InW = 1.0f)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	FORCEINLINE constexpr FVector4(FVector2D InXY, FVector2D InZW)
		: X(InXY.X)
		, Y(InXY.Y)
		, Z(InZW.X)
		, W(InZW.Y)
	{
	}

	explicit FORCEINLINE constexpr FVector4(EForceInit)
		: X(0.f)
		, Y(0.f)
		, Z(0.f)
		, W(0.f)
	{
	}

	FORCEINLINE float& operator[](int32 ComponentIndex)
	{
		checkSlow(ComponentIndex >= 0 && ComponentIndex < 4);
		return (&X)[ComponentIndex];
	}
	FORCEINLINE float operator[](int32 ComponentIndex) const
	{
		checkSlow(ComponentIndex >= 0 && ComponentIndex < 4);
		return (&X)[ComponentIndex];
	}

	FORCEINLINE FVector4 operator-() const
	{
		return FVector4(-X, -Y, -Z, -W);
	}
	FORCEINLINE FVector4 operator+(const FVector4& V) const
	{
		return FVector4(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
	}
	FORCEINLINE FVector4 operator+=(const FVector4& V)
	{
		X += V.X;
		Y += V.Y;
		Z += V.Z;
		W += V.W;
		return *this;
	}
	FORCEINLINE FVector4 operator-(const FVector4& V) const
	{
		return FVector4(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
	}
	FORCEINLINE FVector4 operator-=(const FVector4& V)
	{
		X -= V.X;
		Y -= V.Y;
		Z -= V.Z;
		W -= V.W;
		return *this;
	}
	FORCEINLINE FVector4 operator*(float Scale) const
	{
		return FVector4(X * Scale, Y * Scale, Z * Scale, W * Scale);
	}
	FORCEINLINE FVector4 operator/(float Scale) const
	{
		const float RScale = 1.f / Scale;
		return FVector4(X * RScale, Y * RScale, Z * RScale, W * RScale);
	}
	FORCEINLINE FVector4 operator/(const FVector4& V) const
	{
		return FVector4(X / V.X, Y / V.Y, Z / V.Z, W / V.W);
	}
	FORCEINLINE FVector4 operator*(const FVector4& V) const
	{
		return FVector4(X * V.X, Y * V.Y, Z * V.Z, W * V.W);
	}
	FORCEINLINE FVector4 operator*=(const FVector4& V)
	{
		X *= V.X;
		Y *= V.Y;
		Z *= V.Z;
		W *= V.W;
		return *this;
	}
	FORCEINLINE FVector4 operator/=(const FVector4& V)
	{
		X /= V.X;
		Y /= V.Y;
		Z /= V.Z;
		W /= V.W;
		return *this;
	}
	FORCEINLINE FVector4 operator*=(float S)
	{
		X *= S;
		Y *= S;
		Z *= S;
		W *= S;
		return *this;
	}
	/** Cross product of the XYZ parts (W = 0) (UE). */
	FORCEINLINE FVector4 operator^(const FVector4& V) const
	{
		return FVector4(Y * V.Z - Z * V.Y, Z * V.X - X * V.Z, X * V.Y - Y * V.X, 0.0f);
	}
	FORCEINLINE bool operator==(const FVector4& V) const
	{
		return X == V.X && Y == V.Y && Z == V.Z && W == V.W;
	}
	FORCEINLINE bool operator!=(const FVector4& V) const
	{
		return X != V.X || Y != V.Y || Z != V.Z || W != V.W;
	}

	FORCEINLINE bool Equals(const FVector4& V, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - V.X) <= Tolerance && FMath::Abs(Y - V.Y) <= Tolerance &&
			FMath::Abs(Z - V.Z) <= Tolerance && FMath::Abs(W - V.W) <= Tolerance;
	}

	FORCEINLINE float& Component(int32 Index)
	{
		return (&X)[Index];
	}
	FORCEINLINE float Component(int32 Index) const
	{
		return (&X)[Index];
	}

	FORCEINLINE void Set(float InX, float InY, float InZ, float InW)
	{
		X = InX;
		Y = InY;
		Z = InZ;
		W = InW;
	}

	FORCEINLINE float Size3() const
	{
		return FMath::Sqrt(X * X + Y * Y + Z * Z);
	}
	FORCEINLINE float SizeSquared3() const
	{
		return X * X + Y * Y + Z * Z;
	}
	FORCEINLINE float Size() const
	{
		return FMath::Sqrt(X * X + Y * Y + Z * Z + W * W);
	}
	FORCEINLINE float SizeSquared() const
	{
		return X * X + Y * Y + Z * Z + W * W;
	}

	/** Unit XYZ, W = 0 (UE: GetSafeNormal). */
	FORCEINLINE FVector4 GetSafeNormal(float Tolerance = SMALL_NUMBER) const
	{
		const float SquareSum = X * X + Y * Y + Z * Z;
		if (SquareSum > Tolerance)
		{
			const float Scale = FMath::InvSqrt(SquareSum);
			return FVector4(X * Scale, Y * Scale, Z * Scale, 0.0f);
		}
		return FVector4(0.f);
	}

	FORCEINLINE FVector4 GetUnsafeNormal3() const
	{
		const float Scale = FMath::InvSqrt(X * X + Y * Y + Z * Z);
		return FVector4(X * Scale, Y * Scale, Z * Scale, 0.0f);
	}

	FORCEINLINE bool IsUnit3(float LengthSquaredTolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(1.0f - SizeSquared3()) < LengthSquaredTolerance;
	}

	FORCEINLINE bool IsNearlyZero3(float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X) <= Tolerance && FMath::Abs(Y) <= Tolerance && FMath::Abs(Z) <= Tolerance;
	}

	/** Mirror about a plane normal (W unchanged) (UE: Reflect3). */
	FORCEINLINE FVector4 Reflect3(const FVector4& Normal) const
	{
		return (Normal * (2.0f * Dot3(*this, Normal))) - *this;
	}

	FORCEINLINE bool ContainsNaN() const
	{
		return !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(W);
	}

	FRotator ToOrientationRotator() const;
	FQuat ToOrientationQuat() const;

	FString ToString() const;
	bool InitFromString(const FString& InSourceString);

	static FORCEINLINE float Dot3(const FVector4& V1, const FVector4& V2)
	{
		return V1.X * V2.X + V1.Y * V2.Y + V1.Z * V2.Z;
	}

	static FORCEINLINE float Dot4(const FVector4& V1, const FVector4& V2)
	{
		return V1.X * V2.X + V1.Y * V2.Y + V1.Z * V2.Z + V1.W * V2.W;
	}
};

FORCEINLINE FVector4 operator*(float Scale, const FVector4& V)
{
	return V.operator*(Scale);
}

FORCEINLINE uint32 GetTypeHash(const FVector4& Vector)
{
	return HashCombine(HashCombine(GetTypeHash(Vector.X), GetTypeHash(Vector.Y)),
		HashCombine(GetTypeHash(Vector.Z), GetTypeHash(Vector.W)));
}
