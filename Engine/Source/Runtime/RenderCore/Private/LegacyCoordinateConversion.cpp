#include "LegacyCoordinateConversion.h"

namespace
{
	/** Keeps a scale component at least 1e-4 away from zero, as the legacy model matrix did. */
	float SanitizeScaleComponent(float Value)
	{
		constexpr float Min = 1.0e-4f;
		if (FMath::Abs(Value) < Min)
		{
			return (Value < 0.0f) ? -Min : Min;
		}
		return Value;
	}

	/** Angle in (-180, 180], without a negative zero. */
	float NormalizeDegrees(float Degrees)
	{
		Degrees = FMath::UnwindDegrees(Degrees);
		if (Degrees <= -180.0f)
		{
			Degrees += 360.0f;
		}
		if (Degrees == 0.0f)
		{
			Degrees = 0.0f;
		}
		return Degrees;
	}

	float Atan2Degrees(float Y, float X)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Y, X));
	}
} // namespace

FVector FLegacyCoordinateConversion::ConvertPosition(const FVector& Legacy)
{
	return Legacy * UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ConvertDirection(const FVector& Legacy)
{
	return Legacy;
}

FVector FLegacyCoordinateConversion::ConvertScale(const FVector& Legacy)
{
	return Legacy;
}

float FLegacyCoordinateConversion::ConvertLength(float Metres)
{
	return Metres * UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ConvertExtent(const FVector& Legacy)
{
	return Legacy * UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ToLegacyPosition(const FVector& World)
{
	return World / UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ToLegacyDirection(const FVector& World)
{
	return World;
}

FVector FLegacyCoordinateConversion::ToLegacyScale(const FVector& World)
{
	return World;
}

float FLegacyCoordinateConversion::ToLegacyLength(float WorldLength)
{
	return WorldLength / UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ToLegacyExtent(const FVector& World)
{
	return World / UnitsPerMetre;
}

FQuat FLegacyCoordinateConversion::ConvertEulerXYZ(const FVector& Degrees)
{
	const FQuat RotX(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(Degrees.X));
	const FQuat RotY(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(Degrees.Y));
	const FQuat RotZ(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(Degrees.Z));
	return RotX * RotY * RotZ;
}

FVector FLegacyCoordinateConversion::ToLegacyEulerXYZ(const FQuat& Rotation)
{
	// R = Rx(A) * Ry(B) * Rz(C) acting on column vectors. FMatrix row I is the image of axis I, so R[Row][Col] is
	// M.M[Col][Row].
	const FMatrix M = FQuatRotationMatrix(Rotation);
	const float R00 = M.M[0][0];
	const float R01 = M.M[1][0];
	const float R02 = M.M[2][0];
	const float R10 = M.M[0][1];
	const float R11 = M.M[1][1];
	const float R12 = M.M[2][1];
	const float R22 = M.M[2][2];

	// R02 = sin B, R12 = -sin A cos B, R22 = cos A cos B, R01 = -cos B sin C, R00 = cos B cos C.
	const float CosB = FMath::Sqrt(R12 * R12 + R22 * R22);
	const float B = Atan2Degrees(R02, CosB);
	if (CosB < 1.0e-5f)
	{
		// Gimbal lock: X and Z turn about the same axis, so all of it goes to X.
		const float A = Atan2Degrees(R02 >= 0.0f ? R10 : -R10, R11);
		return FVector(NormalizeDegrees(A), NormalizeDegrees(B), 0.0f);
	}

	const float A = Atan2Degrees(-R12, R22);
	const float C = Atan2Degrees(-R01, R00);
	// The same rotation is also (A + 180, 180 - B, C + 180); keep the triple with the smaller X and Z.
	const FVector First(NormalizeDegrees(A), NormalizeDegrees(B), NormalizeDegrees(C));
	const FVector Second(NormalizeDegrees(A + 180.0f), NormalizeDegrees(180.0f - B), NormalizeDegrees(C + 180.0f));
	const float FirstCost = FMath::Abs(First.X) + FMath::Abs(First.Z);
	const float SecondCost = FMath::Abs(Second.X) + FMath::Abs(Second.Z);
	return SecondCost < FirstCost ? Second : First;
}

FTransform FLegacyCoordinateConversion::ConvertTransform(
	const FVector& Position, const FVector& EulerXYZDegrees, const FVector& Scale)
{
	const FVector SafeScale(
		SanitizeScaleComponent(Scale.X), SanitizeScaleComponent(Scale.Y), SanitizeScaleComponent(Scale.Z));
	return FTransform(ConvertEulerXYZ(EulerXYZDegrees), ConvertPosition(Position), ConvertScale(SafeScale));
}

FQuat FLegacyCoordinateConversion::ConvertLightRotation(float PitchDegrees, float YawDegrees)
{
	const FQuat Yaw(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(YawDegrees));
	const FQuat Pitch(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(PitchDegrees));
	return Yaw * Pitch;
}

void FLegacyCoordinateConversion::ToLegacyLightRotation(
	const FQuat& Rotation, float& OutPitchDegrees, float& OutYawDegrees)
{
	// Legacy light direction: (sin Yaw cos Pitch, -sin Pitch, cos Yaw cos Pitch).
	const FVector Direction = ToLegacyDirection(Rotation.RotateVector(LightForward()));
	const float Length = Direction.Size();
	const FVector Unit = Length > 1.0e-8f ? (Direction / Length) : FVector(0.0f, -1.0f, 0.0f);
	OutPitchDegrees = NormalizeDegrees(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-Unit.Y, -1.0f, 1.0f))));
	OutYawDegrees = NormalizeDegrees(Atan2Degrees(Unit.X, Unit.Z));
}

FVector FLegacyCoordinateConversion::LightForward()
{
	return ConvertDirection(FVector(0.0f, 0.0f, 1.0f));
}
