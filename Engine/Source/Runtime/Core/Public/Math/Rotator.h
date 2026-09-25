#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Templates/TypeHash.h"

/**
 * Orientation as Pitch / Yaw / Roll in degrees (UE: FRotator). Yaw turns around Z (X toward Y), Pitch around Y (X
 * toward Z), Roll around X; applied Roll, then Pitch, then Yaw.
 */
struct CORE_API FRotator
{
	/** Up / down, degrees. */
	float Pitch;

	/** Left / right, degrees. */
	float Yaw;

	/** Around the forward axis, degrees. */
	float Roll;

	static const FRotator ZeroRotator;

	/** Uninitialised (UE). */
	FRotator() = default;

	explicit FORCEINLINE constexpr FRotator(float InF)
		: Pitch(InF)
		, Yaw(InF)
		, Roll(InF)
	{
	}

	FORCEINLINE constexpr FRotator(float InPitch, float InYaw, float InRoll)
		: Pitch(InPitch)
		, Yaw(InYaw)
		, Roll(InRoll)
	{
	}

	explicit FORCEINLINE constexpr FRotator(EForceInit)
		: Pitch(0)
		, Yaw(0)
		, Roll(0)
	{
	}

	explicit FRotator(const FQuat& Quat);

	FORCEINLINE FRotator operator+(const FRotator& R) const
	{
		return FRotator(Pitch + R.Pitch, Yaw + R.Yaw, Roll + R.Roll);
	}
	FORCEINLINE FRotator operator-(const FRotator& R) const
	{
		return FRotator(Pitch - R.Pitch, Yaw - R.Yaw, Roll - R.Roll);
	}
	FORCEINLINE FRotator operator*(float Scale) const
	{
		return FRotator(Pitch * Scale, Yaw * Scale, Roll * Scale);
	}
	FORCEINLINE FRotator operator*=(float Scale)
	{
		Pitch = Pitch * Scale;
		Yaw = Yaw * Scale;
		Roll = Roll * Scale;
		return *this;
	}
	FORCEINLINE bool operator==(const FRotator& R) const
	{
		return Pitch == R.Pitch && Yaw == R.Yaw && Roll == R.Roll;
	}
	FORCEINLINE bool operator!=(const FRotator& V) const
	{
		return Pitch != V.Pitch || Yaw != V.Yaw || Roll != V.Roll;
	}
	FORCEINLINE FRotator operator+=(const FRotator& R)
	{
		Pitch += R.Pitch;
		Yaw += R.Yaw;
		Roll += R.Roll;
		return *this;
	}
	FORCEINLINE FRotator operator-=(const FRotator& R)
	{
		Pitch -= R.Pitch;
		Yaw -= R.Yaw;
		Roll -= R.Roll;
		return *this;
	}

	FORCEINLINE bool IsNearlyZero(float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(NormalizeAxis(Pitch)) <= Tolerance && FMath::Abs(NormalizeAxis(Yaw)) <= Tolerance &&
			FMath::Abs(NormalizeAxis(Roll)) <= Tolerance;
	}

	FORCEINLINE bool IsZero() const
	{
		return (ClampAxis(Pitch) == 0.f) && (ClampAxis(Yaw) == 0.f) && (ClampAxis(Roll) == 0.f);
	}

	/** Same orientation within Tolerance degrees per axis, wrapping at 360 (UE: Equals). */
	FORCEINLINE bool Equals(const FRotator& R, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return (FMath::Abs(NormalizeAxis(Pitch - R.Pitch)) <= Tolerance) &&
			(FMath::Abs(NormalizeAxis(Yaw - R.Yaw)) <= Tolerance) &&
			(FMath::Abs(NormalizeAxis(Roll - R.Roll)) <= Tolerance);
	}

	FORCEINLINE FRotator Add(float DeltaPitch, float DeltaYaw, float DeltaRoll)
	{
		Yaw += DeltaYaw;
		Pitch += DeltaPitch;
		Roll += DeltaRoll;
		return *this;
	}

	/** The inverse rotation (UE: GetInverse). */
	FRotator GetInverse() const;

	FRotator GridSnap(const FRotator& RotGrid) const;

	/** Forward unit vector of this orientation (roll ignored) (UE: Vector). */
	FVector Vector() const;

	/** Same orientation as a quaternion (UE: Quaternion). */
	FQuat Quaternion() const;

	/** (Roll, Pitch, Yaw) as a vector, degrees (UE: Euler). */
	FVector Euler() const;

	/** Rotates a vector (UE: RotateVector). */
	FVector RotateVector(const FVector& V) const;

	/** Applies the inverse rotation (UE: UnrotateVector). */
	FVector UnrotateVector(const FVector& V) const;

	/** Each axis in [0, 360) (UE: Clamp). */
	FORCEINLINE FRotator Clamp() const
	{
		return FRotator(ClampAxis(Pitch), ClampAxis(Yaw), ClampAxis(Roll));
	}

	/** Each axis in (-180, 180] (UE: GetNormalized). */
	FORCEINLINE FRotator GetNormalized() const
	{
		FRotator Rot = *this;
		Rot.Normalize();
		return Rot;
	}

	/** Each axis in [0, 360) (UE: GetDenormalized). */
	FORCEINLINE FRotator GetDenormalized() const
	{
		FRotator Rot = *this;
		Rot.Pitch = ClampAxis(Rot.Pitch);
		Rot.Yaw = ClampAxis(Rot.Yaw);
		Rot.Roll = ClampAxis(Rot.Roll);
		return Rot;
	}

	FORCEINLINE void Normalize()
	{
		Pitch = NormalizeAxis(Pitch);
		Yaw = NormalizeAxis(Yaw);
		Roll = NormalizeAxis(Roll);
	}

	/** Component along an axis. */
	FORCEINLINE float GetComponentForAxis(EAxis::Type Axis) const
	{
		switch (Axis)
		{
			case EAxis::X:
				return Roll;
			case EAxis::Y:
				return Pitch;
			case EAxis::Z:
				return Yaw;
			default:
				return 0.f;
		}
	}

	FORCEINLINE void SetComponentForAxis(EAxis::Type Axis, float Component)
	{
		switch (Axis)
		{
			case EAxis::X:
				Roll = Component;
				break;
			case EAxis::Y:
				Pitch = Component;
				break;
			case EAxis::Z:
				Yaw = Component;
				break;
			default:
				break;
		}
	}

	/** Pitch 180 - P, Yaw + 180, Roll + 180: the same orientation (UE: GetEquivalentRotator). */
	FRotator GetEquivalentRotator() const;

	/** Makes this rotator's axes the closest to MakeClosest's in value (UE: SetClosestToMe). */
	void SetClosestToMe(FRotator& MakeClosest) const;

	/** Sum of the absolute axis differences, wrapped (UE: GetManhattanDistance). */
	float GetManhattanDistance(const FRotator& Rotator) const;

	FORCEINLINE bool ContainsNaN() const
	{
		return !FMath::IsFinite(Pitch) || !FMath::IsFinite(Yaw) || !FMath::IsFinite(Roll);
	}

	/** "P=%f Y=%f R=%f" (UE). */
	FString ToString() const;
	FString ToCompactString() const;
	bool InitFromString(const FString& InSourceString);

	/** Angle in [0, 360) (UE: ClampAxis). */
	static FORCEINLINE float ClampAxis(float Angle)
	{
		// Returns Angle in the range (-360,360).
		Angle = FMath::Fmod(Angle, 360.f);
		if (Angle < 0.f)
		{
			// Shift to [0,360) range.
			Angle += 360.f;
		}
		return Angle;
	}

	/** Angle in (-180, 180] (UE: NormalizeAxis). */
	static FORCEINLINE float NormalizeAxis(float Angle)
	{
		// Returns Angle in the range [0,360).
		Angle = ClampAxis(Angle);
		if (Angle > 180.f)
		{
			// Shift to (-180,180].
			Angle -= 360.f;
		}
		return Angle;
	}

	/** Rotator from (Roll, Pitch, Yaw) degrees (UE: MakeFromEuler). */
	static FRotator MakeFromEuler(const FVector& Euler);
};

FORCEINLINE FRotator operator*(float Scale, const FRotator& R)
{
	return R.operator*(Scale);
}

FORCEINLINE uint32 GetTypeHash(const FRotator& Rotator)
{
	return HashCombine(HashCombine(GetTypeHash(Rotator.Pitch), GetTypeHash(Rotator.Yaw)), GetTypeHash(Rotator.Roll));
}

inline FString LexToString(const FRotator& Rotator)
{
	return Rotator.ToString();
}
