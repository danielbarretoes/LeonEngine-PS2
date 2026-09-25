#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"
#include "Templates/TypeHash.h"

/**
 * Rotation quaternion (UE: FQuat). A * B applies B first, then A (UE). Rotations are expected to be normalized.
 */
struct CORE_API FQuat
{
	float X;
	float Y;
	float Z;
	float W;

	static const FQuat Identity;

	/** Uninitialised (UE). */
	FQuat() = default;

	explicit FORCEINLINE constexpr FQuat(EForceInit ZeroOrNot)
		: X(0)
		, Y(0)
		, Z(0)
		, W(ZeroOrNot == ForceInitToZero ? 0.0f : 1.0f)
	{
	}

	FORCEINLINE constexpr FQuat(float InX, float InY, float InZ, float InW)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	explicit FQuat(const FRotator& R);

	/** Rotation matrix to quaternion; the matrix must not be scaled (UE). */
	explicit FQuat(const FMatrix& M);

	/** Rotation of AngleRad radians about a unit Axis (UE). */
	FQuat(FVector Axis, float AngleRad);

	/** Composition: result rotates by Q first, then by this (UE). */
	FORCEINLINE FQuat operator*(const FQuat& Q) const
	{
		return FQuat(W * Q.X + X * Q.W + Y * Q.Z - Z * Q.Y, W * Q.Y - X * Q.Z + Y * Q.W + Z * Q.X,
			W * Q.Z + X * Q.Y - Y * Q.X + Z * Q.W, W * Q.W - X * Q.X - Y * Q.Y - Z * Q.Z);
	}

	FORCEINLINE FQuat operator*=(const FQuat& Q)
	{
		*this = *this * Q;
		return *this;
	}

	/** Rotates a vector (UE: operator*(FVector)). */
	FORCEINLINE FVector operator*(const FVector& V) const
	{
		return RotateVector(V);
	}

	FORCEINLINE FQuat operator*(const float Scale) const
	{
		return FQuat(Scale * X, Scale * Y, Scale * Z, Scale * W);
	}
	FORCEINLINE FQuat operator*=(const float Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		W *= Scale;
		return *this;
	}
	FORCEINLINE FQuat operator/(const float Scale) const
	{
		const float Recip = 1.0f / Scale;
		return FQuat(X * Recip, Y * Recip, Z * Recip, W * Recip);
	}
	FORCEINLINE FQuat operator+(const FQuat& Q) const
	{
		return FQuat(X + Q.X, Y + Q.Y, Z + Q.Z, W + Q.W);
	}
	FORCEINLINE FQuat operator+=(const FQuat& Q)
	{
		X += Q.X;
		Y += Q.Y;
		Z += Q.Z;
		W += Q.W;
		return *this;
	}
	FORCEINLINE FQuat operator-(const FQuat& Q) const
	{
		return FQuat(X - Q.X, Y - Q.Y, Z - Q.Z, W - Q.W);
	}
	FORCEINLINE FQuat operator-=(const FQuat& Q)
	{
		X -= Q.X;
		Y -= Q.Y;
		Z -= Q.Z;
		W -= Q.W;
		return *this;
	}

	/** Dot product (UE: operator|). */
	FORCEINLINE float operator|(const FQuat& Q) const
	{
		return X * Q.X + Y * Q.Y + Z * Q.Z + W * Q.W;
	}

	FORCEINLINE bool operator==(const FQuat& Q) const
	{
		return X == Q.X && Y == Q.Y && Z == Q.Z && W == Q.W;
	}
	FORCEINLINE bool operator!=(const FQuat& Q) const
	{
		return X != Q.X || Y != Q.Y || Z != Q.Z || W != Q.W;
	}

	/** Same rotation within Tolerance (Q and -Q are the same rotation) (UE: Equals). */
	FORCEINLINE bool Equals(const FQuat& Q, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return (FMath::Abs(X - Q.X) <= Tolerance && FMath::Abs(Y - Q.Y) <= Tolerance &&
				   FMath::Abs(Z - Q.Z) <= Tolerance && FMath::Abs(W - Q.W) <= Tolerance) ||
			(FMath::Abs(X + Q.X) <= Tolerance && FMath::Abs(Y + Q.Y) <= Tolerance && FMath::Abs(Z + Q.Z) <= Tolerance &&
				FMath::Abs(W + Q.W) <= Tolerance);
	}

	FORCEINLINE bool IsIdentity(float Tolerance = SMALL_NUMBER) const
	{
		return Equals(Identity, Tolerance);
	}

	/** Normalizes in place; a degenerate quaternion becomes Identity (UE). */
	FORCEINLINE void Normalize(float Tolerance = SMALL_NUMBER)
	{
		const float SquareSum = X * X + Y * Y + Z * Z + W * W;
		if (SquareSum >= Tolerance)
		{
			const float Scale = FMath::InvSqrt(SquareSum);
			X *= Scale;
			Y *= Scale;
			Z *= Scale;
			W *= Scale;
		}
		else
		{
			*this = FQuat::Identity;
		}
	}

	FORCEINLINE FQuat GetNormalized(float Tolerance = SMALL_NUMBER) const
	{
		FQuat Result(*this);
		Result.Normalize(Tolerance);
		return Result;
	}

	FORCEINLINE bool IsNormalized() const
	{
		return FMath::Abs(1.f - SizeSquared()) < THRESH_QUAT_NORMALIZED;
	}

	FORCEINLINE float Size() const
	{
		return FMath::Sqrt(X * X + Y * Y + Z * Z + W * W);
	}

	FORCEINLINE float SizeSquared() const
	{
		return X * X + Y * Y + Z * Z + W * W;
	}

	/** Rotation angle in radians (UE: GetAngle). */
	FORCEINLINE float GetAngle() const
	{
		return 2.f * FMath::Acos(W);
	}

	/** Axis and angle in radians (UE: ToAxisAndAngle). */
	void ToAxisAndAngle(FVector& Axis, float& Angle) const;

	/** Unit rotation axis ((1, 0, 0) for no rotation) (UE: GetRotationAxis). */
	FVector GetRotationAxis() const;

	/** (Roll, Pitch, Yaw) degrees (UE: Euler). */
	FVector Euler() const;

	static FQuat MakeFromEuler(const FVector& Euler);

	FRotator Rotator() const;

	/** Rotates a vector (UE: RotateVector). */
	FORCEINLINE FVector RotateVector(FVector V) const
	{
		// V' = V + 2w(Q x V) + (2Q x (Q x V)), refactored: T = 2(Q x V); V' = V + w*T + (Q x T).
		const FVector Q(X, Y, Z);
		const FVector T = 2.f * FVector::CrossProduct(Q, V);
		const FVector Result = V + (W * T) + FVector::CrossProduct(Q, T);
		return Result;
	}

	/** Applies the inverse rotation (UE: UnrotateVector). */
	FORCEINLINE FVector UnrotateVector(FVector V) const
	{
		const FVector Q(-X, -Y, -Z); // Inverse
		const FVector T = 2.f * FVector::CrossProduct(Q, V);
		const FVector Result = V + (W * T) + FVector::CrossProduct(Q, T);
		return Result;
	}

	/** Conjugate: the inverse of a unit quaternion (UE: Inverse). */
	FORCEINLINE FQuat Inverse() const
	{
		checkSlow(IsNormalized());
		return FQuat(-X, -Y, -Z, W);
	}

	/** Negates the quaternion when W < 0 (same rotation, W >= 0) (UE: EnforceShortestArcWith is related). */
	FORCEINLINE void EnforceShortestArcWith(const FQuat& OtherQuat)
	{
		const float DotResult = (OtherQuat | *this);
		const float Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);
		X *= Bias;
		Y *= Bias;
		Z *= Bias;
		W *= Bias;
	}

	FORCEINLINE FVector GetAxisX() const
	{
		return RotateVector(FVector(1.f, 0.f, 0.f));
	}
	FORCEINLINE FVector GetAxisY() const
	{
		return RotateVector(FVector(0.f, 1.f, 0.f));
	}
	FORCEINLINE FVector GetAxisZ() const
	{
		return RotateVector(FVector(0.f, 0.f, 1.f));
	}
	FORCEINLINE FVector GetForwardVector() const
	{
		return GetAxisX();
	}
	FORCEINLINE FVector GetRightVector() const
	{
		return GetAxisY();
	}
	FORCEINLINE FVector GetUpVector() const
	{
		return GetAxisZ();
	}
	FORCEINLINE FVector Vector() const
	{
		return GetAxisX();
	}

	/** Angle in radians between two rotations (UE: AngularDistance). */
	FORCEINLINE float AngularDistance(const FQuat& Q) const
	{
		const float InnerProd = X * Q.X + Y * Q.Y + Z * Q.Z + W * Q.W;
		return FMath::Acos((2 * InnerProd * InnerProd) - 1.f);
	}

	FORCEINLINE bool ContainsNaN() const
	{
		return !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(W);
	}

	/** "X=%.9f Y=%.9f Z=%.9f W=%.9f" (UE). */
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);

	/** Rotation taking unit A to unit B (UE: FindBetweenNormals). */
	static FQuat FindBetweenNormals(const FVector& Normal1, const FVector& Normal2);

	/** Rotation taking A to B (any length) (UE: FindBetweenVectors / FindBetween). */
	static FQuat FindBetweenVectors(const FVector& Vector1, const FVector& Vector2);
	static FORCEINLINE FQuat FindBetween(const FVector& Vector1, const FVector& Vector2)
	{
		return FindBetweenVectors(Vector1, Vector2);
	}

	/** Spherical interpolation without the final normalization (UE: Slerp_NotNormalized). */
	static FQuat Slerp_NotNormalized(
		const FQuat& Quat1, const FQuat& Quat2, float Slerp); // NOLINT(readability-identifier-naming): UE name

	/** Spherical interpolation along the shortest arc (UE: Slerp). */
	static FORCEINLINE FQuat Slerp(const FQuat& Quat1, const FQuat& Quat2, float Slerp)
	{
		return Slerp_NotNormalized(Quat1, Quat2, Slerp).GetNormalized();
	}

	/** Normalized linear interpolation along the shortest arc (UE: FastLerp + normalize). */
	static FORCEINLINE FQuat FastLerp(const FQuat& A, const FQuat& B, const float Alpha)
	{
		// To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
		const float DotResult = (A | B);
		const float Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);
		return (B * Alpha) + (A * (Bias * (1.f - Alpha)));
	}

	/** Squared error between two rotations, 0 = equal (UE: Error). */
	static FORCEINLINE float Error(const FQuat& Q1, const FQuat& Q2)
	{
		const float Cosom = FMath::Abs(Q1.X * Q2.X + Q1.Y * Q2.Y + Q1.Z * Q2.Z + Q1.W * Q2.W);
		return (FMath::Abs(Cosom) < 0.9999999f) ? FMath::Acos(Cosom) * (1.f / PI) : 0.0f;
	}
};

FORCEINLINE FQuat operator*(const float Scale, const FQuat& Quat)
{
	return Quat.operator*(Scale);
}

FORCEINLINE uint32 GetTypeHash(const FQuat& Quat)
{
	return HashCombine(
		HashCombine(GetTypeHash(Quat.X), GetTypeHash(Quat.Y)), HashCombine(GetTypeHash(Quat.Z), GetTypeHash(Quat.W)));
}

inline FString LexToString(const FQuat& Quat)
{
	return Quat.ToString();
}

inline FArchive& operator<<(FArchive& Ar, FQuat& V)
{
	return Ar << V.X << V.Y << V.Z << V.W;
}
