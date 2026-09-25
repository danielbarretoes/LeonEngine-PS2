#include "LegacyCoordinateConversion.h"

#include "MeshData.h"

namespace
{
	/** The change of basis without the unit scale: legacy Y and Z swap (its own inverse). */
	FVector SwapYZ(const FVector& V)
	{
		return FVector(V.X, V.Z, V.Y);
	}

	/** A yaw about world +Z (UE: X toward Y), in degrees. */
	FQuat WorldYawQuat(float Degrees)
	{
		return FQuat(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(Degrees));
	}

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
	return SwapYZ(Legacy) * UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ConvertDirection(const FVector& Legacy)
{
	return SwapYZ(Legacy);
}

FVector4 FLegacyCoordinateConversion::ConvertTangent(const FVector4& Legacy)
{
	return FVector4(Legacy.X, Legacy.Z, Legacy.Y, -Legacy.W);
}

FVector FLegacyCoordinateConversion::ConvertScale(const FVector& Legacy)
{
	return SwapYZ(Legacy);
}

float FLegacyCoordinateConversion::ConvertLength(float Metres)
{
	return Metres * UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ConvertExtent(const FVector& Legacy)
{
	return SwapYZ(Legacy) * UnitsPerMetre;
}

FQuat FLegacyCoordinateConversion::ConvertRotation(const FQuat& Legacy)
{
	// The swap S is its own inverse: the world rotation is S * R * S, the same angle about S * axis turned the other
	// way (det S = -1).
	return FQuat(-Legacy.X, -Legacy.Z, -Legacy.Y, Legacy.W);
}

FVector FLegacyCoordinateConversion::ToLegacyPosition(const FVector& World)
{
	return SwapYZ(World) / UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ToLegacyDirection(const FVector& World)
{
	return SwapYZ(World);
}

FVector4 FLegacyCoordinateConversion::ToLegacyTangent(const FVector4& World)
{
	return FVector4(World.X, World.Z, World.Y, -World.W);
}

FVector FLegacyCoordinateConversion::ToLegacyScale(const FVector& World)
{
	return SwapYZ(World);
}

float FLegacyCoordinateConversion::ToLegacyLength(float WorldLength)
{
	return WorldLength / UnitsPerMetre;
}

FVector FLegacyCoordinateConversion::ToLegacyExtent(const FVector& World)
{
	return SwapYZ(World) / UnitsPerMetre;
}

FQuat FLegacyCoordinateConversion::ToLegacyRotation(const FQuat& World)
{
	return FQuat(-World.X, -World.Z, -World.Y, World.W);
}

FQuat FLegacyCoordinateConversion::ConvertEulerXYZ(const FVector& Degrees)
{
	// Legacy axes: the quaternion is built in the legacy basis, then converted.
	const FQuat RotX(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(Degrees.X));
	const FQuat RotY(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(Degrees.Y));
	const FQuat RotZ(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(Degrees.Z));
	return ConvertRotation(RotX * RotY * RotZ);
}

FVector FLegacyCoordinateConversion::ToLegacyEulerXYZ(const FQuat& Rotation)
{
	// R = Rx(A) * Ry(B) * Rz(C) acting on legacy column vectors. FMatrix row I is the image of axis I, so R[Row][Col]
	// is M.M[Col][Row].
	const FMatrix M = FQuatRotationMatrix(ToLegacyRotation(Rotation));
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

FQuat FLegacyCoordinateConversion::ConvertActorEulerXYZ(const FVector& Degrees)
{
	// The local yaw of 90 degrees turns the actor's forward (+X) onto the converted legacy forward (+Y).
	return ConvertEulerXYZ(Degrees) * WorldYawQuat(90.0f);
}

FVector FLegacyCoordinateConversion::ToLegacyActorEulerXYZ(const FQuat& Rotation)
{
	return ToLegacyEulerXYZ(Rotation * WorldYawQuat(-90.0f));
}

float FLegacyCoordinateConversion::ConvertActorYaw(float LegacyYawDegrees)
{
	return 90.0f - LegacyYawDegrees;
}

float FLegacyCoordinateConversion::ToLegacyActorYaw(float WorldYawDegrees)
{
	return NormalizeDegrees(90.0f - WorldYawDegrees);
}

float FLegacyCoordinateConversion::ConvertYawRate(float LegacyDegreesPerSecond)
{
	return -LegacyDegreesPerSecond;
}

float FLegacyCoordinateConversion::ToLegacyYawRate(float WorldDegreesPerSecond)
{
	return -WorldDegreesPerSecond;
}

FRotator FLegacyCoordinateConversion::ConvertOrbitViewRotation(float LegacyYawDegrees, float LegacyPitchDegrees)
{
	// The legacy eye offset converts to FRotator(P, Y, 0).Vector(); the view looks back along it.
	return FRotator(-LegacyPitchDegrees, LegacyYawDegrees + 180.0f, 0.0f);
}

void FLegacyCoordinateConversion::ToLegacyOrbitRotation(
	const FRotator& ViewRotation, float& OutYawDegrees, float& OutPitchDegrees)
{
	OutYawDegrees = NormalizeDegrees(ViewRotation.Yaw - 180.0f);
	OutPitchDegrees = -ViewRotation.Pitch;
}

FRotator FLegacyCoordinateConversion::ConvertFreeLookRotation(float LegacyYawDegrees, float LegacyPitchDegrees)
{
	// The legacy forward converts to FRotator(P, Y, 0).Vector().
	return FRotator(LegacyPitchDegrees, LegacyYawDegrees, 0.0f);
}

void FLegacyCoordinateConversion::ToLegacyFreeLookRotation(
	const FRotator& ViewRotation, float& OutYawDegrees, float& OutPitchDegrees)
{
	OutYawDegrees = NormalizeDegrees(ViewRotation.Yaw);
	OutPitchDegrees = ViewRotation.Pitch;
}

FQuat FLegacyCoordinateConversion::ConvertLightRotation(float PitchDegrees, float YawDegrees)
{
	// Converted, the legacy direction is (cos P sin Y, cos P cos Y, -sin P): the forward of FRotator(-P, 90 - Y, 0).
	return FRotator(-PitchDegrees, 90.0f - YawDegrees, 0.0f).Quaternion();
}

void FLegacyCoordinateConversion::ToLegacyLightRotation(
	const FQuat& Rotation, float& OutPitchDegrees, float& OutYawDegrees)
{
	// Legacy light direction: (sin Yaw cos Pitch, -sin Pitch, cos Yaw cos Pitch).
	const FVector Direction = ToLegacyDirection(Rotation.GetForwardVector());
	const float Length = Direction.Size();
	const FVector Unit = Length > 1.0e-8f ? (Direction / Length) : FVector(0.0f, -1.0f, 0.0f);
	OutPitchDegrees = NormalizeDegrees(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-Unit.Y, -1.0f, 1.0f))));
	OutYawDegrees = NormalizeDegrees(Atan2Degrees(Unit.X, Unit.Z));
}

void FLegacyCoordinateConversion::ConvertMeshData(FMeshData& Data)
{
	for (FVertex& Vertex : Data.Vertices)
	{
		Vertex.Position = ConvertPosition(Vertex.Position);
		Vertex.Normal = ConvertDirection(Vertex.Normal);
		Vertex.Tangent = ConvertTangent(Vertex.Tangent);
	}
}
