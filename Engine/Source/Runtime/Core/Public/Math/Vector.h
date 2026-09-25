#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/Axis.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

/**
 * A 3D vector of floats (UE: FVector, float in UE4). UE axes: X forward, Y right, Z up, left-handed, 1 unit = 1 cm.
 * `|` is the dot product and `^` the cross product (UE).
 */
struct CORE_API FVector
{
	float X;
	float Y;
	float Z;

	static const FVector ZeroVector;
	static const FVector OneVector;
	static const FVector UpVector;
	static const FVector DownVector;
	static const FVector ForwardVector;
	static const FVector BackwardVector;
	static const FVector RightVector;
	static const FVector LeftVector;
	static const FVector XAxisVector;
	static const FVector YAxisVector;
	static const FVector ZAxisVector;

	/** Uninitialised (UE). */
	FVector() = default;

	explicit FORCEINLINE constexpr FVector(float InF)
		: X(InF)
		, Y(InF)
		, Z(InF)
	{
	}

	FORCEINLINE constexpr FVector(float InX, float InY, float InZ)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}

	explicit FORCEINLINE constexpr FVector(EForceInit)
		: X(0.0f)
		, Y(0.0f)
		, Z(0.0f)
	{
	}

	/** From a 2D vector and a Z. */
	explicit FVector(const FVector2D V, float InZ);

	/** Drops W. */
	FVector(const FVector4& V);

	explicit FVector(const FLinearColor& InColor);
	explicit FVector(FIntVector InVector);
	explicit FVector(FIntPoint A);

	// Arithmetic -----------------------------------------------------------------------------------------------------

	/** Cross product (UE: operator^). */
	FORCEINLINE FVector operator^(const FVector& V) const
	{
		return FVector(Y * V.Z - Z * V.Y, Z * V.X - X * V.Z, X * V.Y - Y * V.X);
	}

	static FORCEINLINE FVector CrossProduct(const FVector& A, const FVector& B)
	{
		return A ^ B;
	}

	/** Dot product (UE: operator|). */
	FORCEINLINE float operator|(const FVector& V) const
	{
		return X * V.X + Y * V.Y + Z * V.Z;
	}

	static FORCEINLINE float DotProduct(const FVector& A, const FVector& B)
	{
		return A | B;
	}

	FORCEINLINE FVector operator+(const FVector& V) const
	{
		return FVector(X + V.X, Y + V.Y, Z + V.Z);
	}
	FORCEINLINE FVector operator-(const FVector& V) const
	{
		return FVector(X - V.X, Y - V.Y, Z - V.Z);
	}
	FORCEINLINE FVector operator-(float Bias) const
	{
		return FVector(X - Bias, Y - Bias, Z - Bias);
	}
	FORCEINLINE FVector operator+(float Bias) const
	{
		return FVector(X + Bias, Y + Bias, Z + Bias);
	}
	FORCEINLINE FVector operator*(float Scale) const
	{
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}
	FORCEINLINE FVector operator/(float Scale) const
	{
		const float RScale = 1.f / Scale;
		return FVector(X * RScale, Y * RScale, Z * RScale);
	}
	/** Component-wise product. */
	FORCEINLINE FVector operator*(const FVector& V) const
	{
		return FVector(X * V.X, Y * V.Y, Z * V.Z);
	}
	/** Component-wise division. */
	FORCEINLINE FVector operator/(const FVector& V) const
	{
		return FVector(X / V.X, Y / V.Y, Z / V.Z);
	}
	FORCEINLINE FVector operator-() const
	{
		return FVector(-X, -Y, -Z);
	}
	FORCEINLINE FVector operator+=(const FVector& V)
	{
		X += V.X;
		Y += V.Y;
		Z += V.Z;
		return *this;
	}
	FORCEINLINE FVector operator-=(const FVector& V)
	{
		X -= V.X;
		Y -= V.Y;
		Z -= V.Z;
		return *this;
	}
	FORCEINLINE FVector operator*=(float Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		return *this;
	}
	FORCEINLINE FVector operator/=(float V)
	{
		const float RV = 1.f / V;
		X *= RV;
		Y *= RV;
		Z *= RV;
		return *this;
	}
	FORCEINLINE FVector operator*=(const FVector& V)
	{
		X *= V.X;
		Y *= V.Y;
		Z *= V.Z;
		return *this;
	}
	FORCEINLINE FVector operator/=(const FVector& V)
	{
		X /= V.X;
		Y /= V.Y;
		Z /= V.Z;
		return *this;
	}

	/** Exact equality. */
	FORCEINLINE bool operator==(const FVector& V) const
	{
		return X == V.X && Y == V.Y && Z == V.Z;
	}
	FORCEINLINE bool operator!=(const FVector& V) const
	{
		return X != V.X || Y != V.Y || Z != V.Z;
	}

	/** Equality within Tolerance per component. */
	FORCEINLINE bool Equals(const FVector& V, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - V.X) <= Tolerance && FMath::Abs(Y - V.Y) <= Tolerance && FMath::Abs(Z - V.Z) <= Tolerance;
	}

	FORCEINLINE bool AllComponentsEqual(float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - Y) <= Tolerance && FMath::Abs(X - Z) <= Tolerance && FMath::Abs(Y - Z) <= Tolerance;
	}

	FORCEINLINE float& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < 3);
		return (&X)[Index];
	}
	FORCEINLINE float operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < 3);
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

	/** Component along an axis. */
	float GetComponentForAxis(EAxis::Type Axis) const;
	void SetComponentForAxis(EAxis::Type Axis, float Component);

	FORCEINLINE void Set(float InX, float InY, float InZ)
	{
		X = InX;
		Y = InY;
		Z = InZ;
	}

	// Measures -------------------------------------------------------------------------------------------------------

	FORCEINLINE float GetMax() const
	{
		return FMath::Max(FMath::Max(X, Y), Z);
	}
	FORCEINLINE float GetAbsMax() const
	{
		return FMath::Max(FMath::Max(FMath::Abs(X), FMath::Abs(Y)), FMath::Abs(Z));
	}
	FORCEINLINE float GetMin() const
	{
		return FMath::Min(FMath::Min(X, Y), Z);
	}
	FORCEINLINE float GetAbsMin() const
	{
		return FMath::Min(FMath::Min(FMath::Abs(X), FMath::Abs(Y)), FMath::Abs(Z));
	}
	FORCEINLINE FVector ComponentMin(const FVector& Other) const
	{
		return FVector(FMath::Min(X, Other.X), FMath::Min(Y, Other.Y), FMath::Min(Z, Other.Z));
	}
	FORCEINLINE FVector ComponentMax(const FVector& Other) const
	{
		return FVector(FMath::Max(X, Other.X), FMath::Max(Y, Other.Y), FMath::Max(Z, Other.Z));
	}
	FORCEINLINE FVector GetAbs() const
	{
		return FVector(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
	}

	FORCEINLINE float Size() const
	{
		return FMath::Sqrt(X * X + Y * Y + Z * Z);
	}
	FORCEINLINE float SizeSquared() const
	{
		return X * X + Y * Y + Z * Z;
	}
	FORCEINLINE float Size2D() const
	{
		return FMath::Sqrt(X * X + Y * Y);
	}
	FORCEINLINE float SizeSquared2D() const
	{
		return X * X + Y * Y;
	}

	FORCEINLINE bool IsNearlyZero(float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X) <= Tolerance && FMath::Abs(Y) <= Tolerance && FMath::Abs(Z) <= Tolerance;
	}
	FORCEINLINE bool IsZero() const
	{
		return X == 0.f && Y == 0.f && Z == 0.f;
	}

	/** Length within LengthSquaredTolerance of 1 (UE: IsUnit). */
	FORCEINLINE bool IsUnit(float LengthSquaredTolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(1.0f - SizeSquared()) < LengthSquaredTolerance;
	}

	FORCEINLINE bool IsNormalized() const
	{
		return FMath::Abs(1.f - SizeSquared()) < THRESH_VECTOR_NORMALIZED;
	}

	/** Normalizes in place when longer than Tolerance; returns false (unchanged) otherwise. */
	FORCEINLINE bool Normalize(float Tolerance = SMALL_NUMBER)
	{
		const float SquareSum = X * X + Y * Y + Z * Z;
		if (SquareSum > Tolerance)
		{
			const float Scale = FMath::InvSqrt(SquareSum);
			X *= Scale;
			Y *= Scale;
			Z *= Scale;
			return true;
		}
		return false;
	}

	/** Unit vector, or ZeroVector when too short (UE: GetSafeNormal). */
	FORCEINLINE FVector GetSafeNormal(float Tolerance = SMALL_NUMBER) const
	{
		const float SquareSum = X * X + Y * Y + Z * Z;
		// Not sure if it's safe to add tolerance in there. Might introduce too many errors.
		if (SquareSum == 1.f)
		{
			return *this;
		}
		if (SquareSum < Tolerance)
		{
			return FVector(0.f, 0.f, 0.f);
		}
		const float Scale = FMath::InvSqrt(SquareSum);
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}

	/** Normal in the XY plane (Z = 0) (UE: GetSafeNormal2D). */
	FORCEINLINE FVector GetSafeNormal2D(float Tolerance = SMALL_NUMBER) const
	{
		const float SquareSum = X * X + Y * Y;
		if (SquareSum == 1.f)
		{
			return Z == 0.f ? *this : FVector(X, Y, 0.f);
		}
		if (SquareSum < Tolerance)
		{
			return FVector(0.f, 0.f, 0.f);
		}
		const float Scale = FMath::InvSqrt(SquareSum);
		return FVector(X * Scale, Y * Scale, 0.f);
	}

	/** Unit vector without the zero check (UE: GetUnsafeNormal). */
	FORCEINLINE FVector GetUnsafeNormal() const
	{
		const float Scale = FMath::InvSqrt(X * X + Y * Y + Z * Z);
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}

	/** Splits into a direction and a length (UE: ToDirectionAndLength). */
	void ToDirectionAndLength(FVector& OutDir, float& OutLength) const;

	/** Length clamped to [Min, Max] (UE: GetClampedToSize). */
	FVector GetClampedToSize(float Min, float Max) const;
	FVector GetClampedToSize2D(float Min, float Max) const;
	FVector GetClampedToMaxSize(float MaxSize) const;
	FVector GetClampedToMaxSize2D(float MaxSize) const;

	/** Each component clamped to [-Radius, Radius] (UE: BoundToCube). */
	FVector BoundToCube(float Radius) const;

	/** Each component clamped to [Min, Max] (UE: BoundToBox). */
	FVector BoundToBox(const FVector& Min, const FVector& Max) const;

	/** -1 / +1 per component (UE: GetSignVector). */
	FORCEINLINE FVector GetSignVector() const
	{
		return FVector(
			FMath::FloatSelect(X, 1.f, -1.f), FMath::FloatSelect(Y, 1.f, -1.f), FMath::FloatSelect(Z, 1.f, -1.f));
	}

	/** (X/Z, Y/Z, 1) (UE: Projection). */
	FORCEINLINE FVector Projection() const
	{
		const float RZ = 1.f / Z;
		return FVector(X * RZ, Y * RZ, 1);
	}

	/** 1 / each component (zero components become BIG_NUMBER) (UE: Reciprocal). */
	FVector Reciprocal() const;

	/** Projection onto another vector (UE: ProjectOnTo). */
	FORCEINLINE FVector ProjectOnTo(const FVector& A) const
	{
		return (A * ((*this | A) / (A | A)));
	}

	/** Projection onto a unit vector (UE: ProjectOnToNormal). */
	FORCEINLINE FVector ProjectOnToNormal(const FVector& Normal) const
	{
		return (Normal * (*this | Normal));
	}

	/** Mirror about a plane normal (UE: MirrorByVector). */
	FORCEINLINE FVector MirrorByVector(const FVector& MirrorNormal) const
	{
		return *this - MirrorNormal * (2.f * (*this | MirrorNormal));
	}

	/** Mirror about a plane (UE: MirrorByPlane). */
	FVector MirrorByPlane(const FPlane& Plane) const;

	/** Rotation of this vector about an axis by AngleDeg (UE: RotateAngleAxis). */
	FVector RotateAngleAxis(const float AngleDeg, const FVector& Axis) const;

	/** Rotator whose forward vector is this direction (roll 0) (UE: ToOrientationRotator / Rotation). */
	FRotator ToOrientationRotator() const;
	FRotator Rotation() const;
	FQuat ToOrientationQuat() const;

	/** Angle in the XY plane, radians (UE: HeadingAngle). */
	float HeadingAngle() const;

	/** Two axes orthonormal to this vector (UE: FindBestAxisVectors). */
	void FindBestAxisVectors(FVector& Axis1, FVector& Axis2) const;

	/** Unwinds each component as an angle in degrees to (-180, 180] (UE: UnwindEuler). */
	void UnwindEuler();

	bool ContainsNaN() const
	{
		return !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z);
	}

	/** "X=%3.3f Y=%3.3f Z=%3.3f" (UE). */
	FString ToString() const;
	FString ToCompactString() const;

	/** Parses the ToString format; false when a component is missing (UE: InitFromString). */
	bool InitFromString(const FString& InSourceString);

	// Static helpers -------------------------------------------------------------------------------------------------

	static FORCEINLINE float Dist(const FVector& V1, const FVector& V2)
	{
		return FMath::Sqrt(FVector::DistSquared(V1, V2));
	}
	static FORCEINLINE float Distance(const FVector& V1, const FVector& V2)
	{
		return Dist(V1, V2);
	}
	static FORCEINLINE float DistXY(const FVector& V1, const FVector& V2)
	{
		return FMath::Sqrt(FVector::DistSquaredXY(V1, V2));
	}
	static FORCEINLINE float Dist2D(const FVector& V1, const FVector& V2)
	{
		return DistXY(V1, V2);
	}
	static FORCEINLINE float DistSquared(const FVector& V1, const FVector& V2)
	{
		return FMath::Square(V2.X - V1.X) + FMath::Square(V2.Y - V1.Y) + FMath::Square(V2.Z - V1.Z);
	}
	static FORCEINLINE float DistSquaredXY(const FVector& V1, const FVector& V2)
	{
		return FMath::Square(V2.X - V1.X) + FMath::Square(V2.Y - V1.Y);
	}
	static FORCEINLINE float DistSquared2D(const FVector& V1, const FVector& V2)
	{
		return DistSquaredXY(V1, V2);
	}

	/** Cosine of the angle between A and B in the XY plane (UE: CosineAngle2D). */
	static float CosineAngle2D(FVector A, FVector B);

	/** A projected onto the plane with normal PlaneNormal (UE: VectorPlaneProject). */
	static FORCEINLINE FVector VectorPlaneProject(const FVector& V, const FVector& PlaneNormal)
	{
		return V - V.ProjectOnToNormal(PlaneNormal);
	}

	/** Point projected onto a plane through PlaneBase (UE: PointPlaneProject). */
	static FORCEINLINE FVector PointPlaneProject(
		const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNormal)
	{
		// Find the distance of X from the plane, add the distance back along the normal from the point.
		return Point - PlaneNormal * FVector::PointPlaneDist(Point, PlaneBase, PlaneNormal);
	}

	/** Signed distance of Point from the plane (UE: PointPlaneDist). */
	static FORCEINLINE float PointPlaneDist(const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNormal)
	{
		return (Point - PlaneBase) | PlaneNormal;
	}

	/** Same point within THRESH_POINTS_ARE_SAME (UE: PointsAreSame). */
	static bool PointsAreSame(const FVector& P, const FVector& Q);
	static bool PointsAreNear(const FVector& Point1, const FVector& Point2, float Dist);

	/** Parallel unit normals (UE: Parallel). */
	static FORCEINLINE bool Parallel(
		const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL)
	{
		const float NormalDot = Normal1 | Normal2;
		return FMath::Abs(NormalDot) >= ParallelCosineThreshold;
	}

	static FORCEINLINE bool Coincident(
		const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL)
	{
		const float NormalDot = Normal1 | Normal2;
		return NormalDot >= ParallelCosineThreshold;
	}

	static FORCEINLINE bool Orthogonal(
		const FVector& Normal1, const FVector& Normal2, float OrthogonalCosineThreshold = THRESH_NORMALS_ARE_ORTHOGONAL)
	{
		const float NormalDot = Normal1 | Normal2;
		return FMath::Abs(NormalDot) <= OrthogonalCosineThreshold;
	}

	/** Scalar triple product (UE: Triple). */
	static FORCEINLINE float Triple(const FVector& X, const FVector& Y, const FVector& Z)
	{
		return ((X.X * (Y.Y * Z.Z - Y.Z * Z.Y)) + (X.Y * (Y.Z * Z.X - Y.X * Z.Z)) + (X.Z * (Y.X * Z.Y - Y.Y * Z.X)));
	}

	static FORCEINLINE FVector RadiansToDegrees(const FVector& RadVector)
	{
		return RadVector * (180.f / PI);
	}

	static FORCEINLINE FVector DegreesToRadians(const FVector& DegVector)
	{
		return DegVector * (PI / 180.f);
	}
};

FORCEINLINE FVector operator*(float Scale, const FVector& V)
{
	return V.operator*(Scale);
}

FORCEINLINE uint32 GetTypeHash(const FVector& Vector)
{
	return HashCombine(HashCombine(GetTypeHash(Vector.X), GetTypeHash(Vector.Y)), GetTypeHash(Vector.Z));
}

inline FString LexToString(const FVector& Vector)
{
	return Vector.ToString();
}

template <>
struct TIsPODType<FVector>
{
	enum
	{
		Value = true
	};
};
