#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMath.h"
#include "Math/MathFwd.h"

// Math constants (UE: Math/UnrealMathUtility.h). Float only: Leon has no double-precision math.

#undef PI
#define PI (3.1415926535897932f)
#define SMALL_NUMBER (1.e-8f)
#define KINDA_SMALL_NUMBER (1.e-4f)
#define BIG_NUMBER (3.4e+38f)
#define EULERS_NUMBER (2.71828182845904523536f)
#define UE_GOLDEN_RATIO (1.6180339887498948482045868343656381f)
#define FLOAT_NON_FRACTIONAL (8388608.f)

#define MAX_FLT 3.402823466e+38F

#define INV_PI (0.31830988618f)
#define UE_SQRT_2 (1.4142135623730950488016887242097f)
#define UE_INV_SQRT_2 (0.70710678118654752440084436210485f)
#define HALF_PI (1.57079632679f)
#define TWO_PI (6.28318530717f)

#define DELTA (0.00001f)

// Thresholds used by the geometry code (UE).
#define THRESH_POINT_ON_PLANE (0.10f)
#define THRESH_POINT_ON_SIDE (0.20f)
#define THRESH_POINTS_ARE_SAME (0.00002f)
#define THRESH_POINTS_ARE_NEAR (0.015f)
#define THRESH_NORMALS_ARE_SAME (0.00002f)
#define THRESH_UVS_ARE_SAME (0.0009765625f)
#define THRESH_VECTORS_ARE_NEAR (0.0004f)
#define THRESH_SPLIT_POLY_WITH_PLANE (0.25f)
#define THRESH_SPLIT_POLY_PRECISELY (0.01f)
#define THRESH_ZERO_NORM_SQUARED (0.0001f)
#define THRESH_NORMALS_ARE_PARALLEL (0.999845f)
#define THRESH_NORMALS_ARE_ORTHOGONAL (0.017455f)
#define THRESH_VECTOR_NORMALIZED (0.01f)
#define THRESH_QUAT_NORMALIZED (0.01f)

/** Engine math helpers (UE: FMath). */
struct FMath : public FPlatformMath
{
	// Random numbers -------------------------------------------------------------------------------------------------

	/** Random integer in [0, A) (UE: RandHelper). */
	static FORCEINLINE int32 RandHelper(int32 A)
	{
		return A > 0 ? Min(TruncToInt(FRand() * float(A)), A - 1) : 0;
	}

	/** Random integer in [Min, Max] (UE: RandRange). */
	static FORCEINLINE int32 RandRange(int32 InMin, int32 InMax)
	{
		const int32 Range = (InMax - InMin) + 1;
		return InMin + RandHelper(Range);
	}

	/** Random float in [Min, Max) (UE: FRandRange / RandRange). */
	static FORCEINLINE float FRandRange(float InMin, float InMax)
	{
		return InMin + (InMax - InMin) * FRand();
	}

	static FORCEINLINE float RandRange(float InMin, float InMax)
	{
		return FRandRange(InMin, InMax);
	}

	/** True with probability 0.5 (UE: RandBool). */
	static FORCEINLINE bool RandBool()
	{
		return RandRange(0, 1) == 1;
	}

	// Generic helpers ------------------------------------------------------------------------------------------------

	template <class T>
	static constexpr FORCEINLINE T Clamp(const T X, const T InMin, const T InMax)
	{
		return X < InMin ? InMin : (X < InMax ? X : InMax);
	}

	template <class T>
	static constexpr FORCEINLINE T Max3(const T A, const T B, const T C)
	{
		return Max(Max(A, B), C);
	}

	template <class T>
	static constexpr FORCEINLINE T Min3(const T A, const T B, const T C)
	{
		return Min(Min(A, B), C);
	}

	template <class T>
	static constexpr FORCEINLINE T Square(const T A)
	{
		return A * A;
	}

	template <class T>
	static constexpr FORCEINLINE T Cube(const T A)
	{
		return A * A * A;
	}

	template <class T>
	static constexpr FORCEINLINE bool IsPowerOfTwo(T Value)
	{
		return (Value & (Value - 1)) == static_cast<T>(0);
	}

	template <class T>
	static constexpr FORCEINLINE T DivideAndRoundUp(T Dividend, T Divisor)
	{
		return (Dividend + Divisor - 1) / Divisor;
	}

	template <class T>
	static constexpr FORCEINLINE T DivideAndRoundDown(T Dividend, T Divisor)
	{
		return Dividend / Divisor;
	}

	/** Snaps a value to the nearest grid multiple (UE: GridSnap). */
	static FORCEINLINE float GridSnap(float Location, float Grid)
	{
		if (Grid == 0.0f)
		{
			return Location;
		}
		return FloorToFloat((Location + 0.5f * Grid) / Grid) * Grid;
	}

	/** True when X is in [Min, Max) (UE: IsWithin). */
	template <class ValueType, class BoundsType>
	static FORCEINLINE bool IsWithin(const ValueType& TestValue, const BoundsType& MinValue, const BoundsType& MaxValue)
	{
		return (TestValue >= MinValue) && (TestValue < MaxValue);
	}

	/** True when X is in [Min, Max] (UE: IsWithinInclusive). */
	template <class ValueType, class BoundsType>
	static FORCEINLINE bool IsWithinInclusive(
		const ValueType& TestValue, const BoundsType& MinValue, const BoundsType& MaxValue)
	{
		return (TestValue >= MinValue) && (TestValue <= MaxValue);
	}

	static FORCEINLINE bool IsNearlyEqual(float A, float B, float ErrorTolerance = SMALL_NUMBER)
	{
		return Abs(A - B) <= ErrorTolerance;
	}

	static FORCEINLINE bool IsNearlyZero(float Value, float ErrorTolerance = SMALL_NUMBER)
	{
		return Abs(Value) <= ErrorTolerance;
	}

	/** Sine and cosine together (UE: SinCos; approximated like UE: < 0.001 error). */
	static FORCEINLINE void SinCos(float* ScalarSin, float* ScalarCos, float Value)
	{
		*ScalarSin = Sin(Value);
		*ScalarCos = Cos(Value);
	}

	/** Asin with a polynomial; max error about 7e-5 rad like UE (UE: FastAsin). */
	static FORCEINLINE float FastAsin(float Value)
	{
		// Clamp input to [-1,1].
		const bool bNonnegative = (Value >= 0.0f);
		const float X = Abs(Value);
		float OmX = 1.0f - X;
		if (OmX < 0.0f)
		{
			OmX = 0.0f;
		}
		const float Root = Sqrt(OmX);
		// 7-degree minimax approximation.
		float Result =
			((((((-0.0012624911f * X + 0.0066700901f) * X - 0.0170881256f) * X + 0.0308918810f) * X - 0.0501743046f) *
					 X +
				 0.0889789874f) *
					X -
				0.2145988016f) *
				X +
			1.5707963050f;
		Result *= Root; // acos(|x|)
		// acos(x) = pi - acos(-x) when x < 0, asin(x) = pi/2 - acos(x)
		return (bNonnegative ? 1.5707963050f - Result : Result - 1.5707963050f);
	}

	template <class T>
	static constexpr FORCEINLINE auto RadiansToDegrees(T const& RadVal) -> decltype(RadVal * (180.f / PI))
	{
		return RadVal * (180.f / PI);
	}

	template <class T>
	static constexpr FORCEINLINE auto DegreesToRadians(T const& DegVal) -> decltype(DegVal * (PI / 180.f))
	{
		return DegVal * (PI / 180.f);
	}

	/** Linear interpolation (UE: Lerp). */
	template <class T, class U>
	static FORCEINLINE T Lerp(const T& A, const T& B, const U& Alpha)
	{
		return (T)(A + Alpha * (B - A));
	}

	/** Alpha of Value between A and B (UE: GetRangePct). */
	static FORCEINLINE float GetRangePct(float MinValue, float MaxValue, float Value)
	{
		const float Divisor = MaxValue - MinValue;
		if (IsNearlyZero(Divisor))
		{
			return (Value >= MaxValue) ? 1.f : 0.f;
		}
		return (Value - MinValue) / Divisor;
	}

	/** Maps Value from one range to another, clamped (UE: GetMappedRangeValueClamped). */
	static FORCEINLINE float GetMappedRangeValueClamped(
		float InMin, float InMax, float OutMin, float OutMax, float Value)
	{
		const float ClampedPct = Clamp(GetRangePct(InMin, InMax, Value), 0.f, 1.f);
		return Lerp(OutMin, OutMax, ClampedPct);
	}

	/** Maps Value from one range to another (UE: GetMappedRangeValueUnclamped). */
	static FORCEINLINE float GetMappedRangeValueUnclamped(
		float InMin, float InMax, float OutMin, float OutMax, float Value)
	{
		return Lerp(OutMin, OutMax, GetRangePct(InMin, InMax, Value));
	}

	/** Hermite smooth step between A and B (UE: SmoothStep). */
	static FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		if (X < A)
		{
			return 0.0f;
		}
		if (X >= B)
		{
			return 1.0f;
		}
		const float InterpFraction = (X - A) / (B - A);
		return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
	}

	/** Moves Current toward Target at InterpSpeed per second, exponentially (UE: FInterpTo). */
	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed);

	/** Moves Current toward Target at a constant rate (UE: FInterpConstantTo). */
	static float FInterpConstantTo(float Current, float Target, float DeltaTime, float InterpSpeed);

	/** Eases between A and B (UE: InterpEaseIn / InterpEaseOut / InterpEaseInOut). */
	static float InterpEaseIn(float A, float B, float Alpha, float Exp);
	static float InterpEaseOut(float A, float B, float Alpha, float Exp);
	static float InterpEaseInOut(float A, float B, float Alpha, float Exp);

	/** Angle in (-180, 180] (UE: UnwindDegrees). */
	static FORCEINLINE float UnwindDegrees(float A)
	{
		while (A > 180.f)
		{
			A -= 360.f;
		}
		while (A < -180.f)
		{
			A += 360.f;
		}
		return A;
	}

	/** Angle in (-PI, PI] (UE: UnwindRadians). */
	static FORCEINLINE float UnwindRadians(float A)
	{
		while (A > PI)
		{
			A -= TWO_PI;
		}
		while (A < -PI)
		{
			A += TWO_PI;
		}
		return A;
	}

	/** Clamps an angle in degrees to [MinAngle, MaxAngle] across the wrap (UE: ClampAngle). */
	static float ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);

	// Vector / rotation interpolation (UE: VInterpTo, RInterpTo, QInterpTo and the constant-rate versions).
	static FVector VInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	static FVector VInterpConstantTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	static FRotator RInterpTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	static FRotator RInterpConstantTo(
		const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	static FQuat QInterpTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed);
	static FQuat QInterpConstantTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed);

	/** Uniformly distributed random unit vector (UE: VRand). */
	static FVector VRand();

	/** Random unit vector inside a cone around Dir (UE: VRandCone). */
	static FVector VRandCone(const FVector& Dir, float ConeHalfAngleRad);

	// Geometry
	// ---------------------------------------------------------------------------------------------------------

	/** Point of the segment closest to Point (UE: ClosestPointOnSegment). */
	static FVector ClosestPointOnSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);

	static float PointDistToSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	static float PointDistToSegmentSquared(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);

	/** Distance from Point to the infinite line through Origin (UE: PointDistToLine). */
	static float PointDistToLine(const FVector& Point, const FVector& Direction, const FVector& Origin);

	/** Intersection of the line through Point1 and Point2 with a plane (UE: LinePlaneIntersection). */
	static FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FPlane& Plane);
	static FVector LinePlaneIntersection(
		const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal);

	/** Segment Start-End against a box; Direction is End - Start (UE: LineBoxIntersection). */
	static bool LineBoxIntersection(
		const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction);
	static bool LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction,
		const FVector& OneOverDirection);

	/** Sphere against box (UE: SphereAABBIntersection). */
	static bool SphereAABBIntersection(const FVector& SphereCenter, float RadiusSquared, const FBox& AABB);

	/** Direction mirrored by a surface (UE: GetReflectionVector). */
	static FVector GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal);

	/** Shortest signed difference B - A of two angles in degrees (UE: FindDeltaAngleDegrees). */
	static FORCEINLINE float FindDeltaAngleDegrees(float A1, float A2)
	{
		// Find the difference.
		float Delta = A2 - A1;
		// If change is larger than 180...
		if (Delta > 180.0f)
		{
			// Flip to negative equivalent.
			Delta = Delta - 360.0f;
		}
		else if (Delta < -180.0f)
		{
			// Otherwise, if change is smaller than -180, flip to positive equivalent.
			Delta = Delta + 360.0f;
		}
		return Delta;
	}

	static FORCEINLINE float FindDeltaAngleRadians(float A1, float A2)
	{
		float Delta = A2 - A1;
		if (Delta > PI)
		{
			Delta = Delta - TWO_PI;
		}
		else if (Delta < -PI)
		{
			Delta = Delta + TWO_PI;
		}
		return Delta;
	}

	/** Truncates to a float representable integer boundary (UE: TruncateToHalfIfClose). */
	static FORCEINLINE float TruncateToHalfIfClose(float F, float Tolerance = SMALL_NUMBER)
	{
		float ValueToFudgeIntegralPart = 0.0f;
		const float ValueToFudgeFractionalPart = Modf(F, &ValueToFudgeIntegralPart);
		if (F < 0.0f)
		{
			return ValueToFudgeIntegralPart +
				((IsNearlyEqual(ValueToFudgeFractionalPart, -0.5f, Tolerance)) ? -0.5f : ValueToFudgeFractionalPart);
		}
		return ValueToFudgeIntegralPart +
			((IsNearlyEqual(ValueToFudgeFractionalPart, 0.5f, Tolerance)) ? 0.5f : ValueToFudgeFractionalPart);
	}
};
