#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Serialization/Archive.h"

/** Plane X*x + Y*y + Z*z = W with a unit normal (X, Y, Z) (UE: FPlane, derives from FVector). */
struct CORE_API FPlane : public FVector
{
	/** Distance from the origin along the normal. */
	float W;

	/** Uninitialised (UE). */
	FPlane() = default;

	FORCEINLINE FPlane(const FVector4& V)
		: FVector(V)
		, W(V.W)
	{
	}

	FORCEINLINE constexpr FPlane(float InX, float InY, float InZ, float InW)
		: FVector(InX, InY, InZ)
		, W(InW)
	{
	}

	FORCEINLINE FPlane(FVector InNormal, float InW)
		: FVector(InNormal)
		, W(InW)
	{
	}

	/** Plane through InBase with normal InNormal. */
	FORCEINLINE FPlane(FVector InBase, const FVector& InNormal)
		: FVector(InNormal)
		, W(InBase | InNormal)
	{
	}

	/** Plane through three points, normal (B - A) ^ (C - A). */
	FPlane(FVector A, FVector B, FVector C);

	explicit FORCEINLINE constexpr FPlane(EForceInit)
		: FVector(ForceInit)
		, W(0.f)
	{
	}

	/** Signed distance of P from the plane (UE: PlaneDot). */
	FORCEINLINE float PlaneDot(const FVector& P) const
	{
		return X * P.X + Y * P.Y + Z * P.Z - W;
	}

	/** Normalizes the normal (and W); false when the normal is too short. */
	bool Normalize(float Tolerance = SMALL_NUMBER);

	/** The same plane facing the other way. */
	FORCEINLINE FPlane Flip() const
	{
		return FPlane(-X, -Y, -Z, -W);
	}

	/** Plane transformed by a matrix (UE: TransformBy). */
	FPlane TransformBy(const FMatrix& M) const;

	/** Transform with a precomputed transpose adjoint (UE: TransformByUsingAdjointT). */
	FPlane TransformByUsingAdjointT(const FMatrix& M, float DetM, const FMatrix& TA) const;

	FORCEINLINE bool operator==(const FPlane& V) const
	{
		return X == V.X && Y == V.Y && Z == V.Z && W == V.W;
	}
	FORCEINLINE bool operator!=(const FPlane& V) const
	{
		return X != V.X || Y != V.Y || Z != V.Z || W != V.W;
	}

	FORCEINLINE bool Equals(const FPlane& V, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - V.X) < Tolerance && FMath::Abs(Y - V.Y) < Tolerance && FMath::Abs(Z - V.Z) < Tolerance &&
			FMath::Abs(W - V.W) < Tolerance;
	}

	/** Dot product of the four components. */
	FORCEINLINE float operator|(const FPlane& V) const
	{
		return X * V.X + Y * V.Y + Z * V.Z + W * V.W;
	}
	FORCEINLINE FPlane operator+(const FPlane& V) const
	{
		return FPlane(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
	}
	FORCEINLINE FPlane operator-(const FPlane& V) const
	{
		return FPlane(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
	}
	FORCEINLINE FPlane operator/(float Scale) const
	{
		const float RScale = 1.f / Scale;
		return FPlane(X * RScale, Y * RScale, Z * RScale, W * RScale);
	}
	FORCEINLINE FPlane operator*(float Scale) const
	{
		return FPlane(X * Scale, Y * Scale, Z * Scale, W * Scale);
	}
	FORCEINLINE FPlane operator*(const FPlane& V)
	{
		return FPlane(X * V.X, Y * V.Y, Z * V.Z, W * V.W);
	}
	FORCEINLINE FPlane operator+=(const FPlane& V)
	{
		X += V.X;
		Y += V.Y;
		Z += V.Z;
		W += V.W;
		return *this;
	}
	FORCEINLINE FPlane operator-=(const FPlane& V)
	{
		X -= V.X;
		Y -= V.Y;
		Z -= V.Z;
		W -= V.W;
		return *this;
	}
	FORCEINLINE FPlane operator*=(float Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		W *= Scale;
		return *this;
	}
	FORCEINLINE FPlane operator/=(float V)
	{
		const float RV = 1.f / V;
		X *= RV;
		Y *= RV;
		Z *= RV;
		W *= RV;
		return *this;
	}

	FString ToString() const;
};

inline FArchive& operator<<(FArchive& Ar, FPlane& V)
{
	return Ar << static_cast<FVector&>(V) << V.W;
}
