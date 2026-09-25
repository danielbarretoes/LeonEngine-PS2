#pragma once

#include "CoreMinimal.h"
#include "Level/LegacyTransform.h"

constexpr int32 MaxDirectionalLights = 2;
constexpr int32 MaxPointLights = 4;

/** UE FDirectionalLight Source Angle default (~sun disc), in degrees. */
constexpr float DefaultLightSourceAngleDegrees = 0.5357f;

/** Light travel direction from UE-like pitch (X) / yaw (Y) degrees. Roll ignored. */
[[nodiscard]] inline FVector LightDirectionFromRotation(const FVector& RotationDegrees)
{
	constexpr float DegToRad = 0.017453292519943295769f;
	const float Pitch = RotationDegrees.X * DegToRad;
	const float Yaw = RotationDegrees.Y * DegToRad;
	const float Cp = FMath::Cos(Pitch);
	const FVector Dir(FMath::Sin(Yaw) * Cp, -FMath::Sin(Pitch), FMath::Cos(Yaw) * Cp);
	const float Len = Dir.Size();
	return Len > 1.0e-8f ? (Dir / Len) : FVector(0.0f, -1.0f, 0.0f);
}

/** Inverse of LightDirectionFromRotation (roll = 0). */
[[nodiscard]] inline FVector RotationFromLightDirection(const FVector& Direction)
{
	constexpr float RadToDeg = 57.295779513082320877f;
	const float Len = Direction.Size();
	const FVector D = Len > 1.0e-8f ? (Direction / Len) : FVector(0.0f, -1.0f, 0.0f);
	const float Pitch = FMath::Asin(FMath::Clamp(-D.Y, -1.0f, 1.0f));
	const float Yaw = FMath::Atan2(D.X, D.Z);
	return FVector(Pitch * RadToDeg, Yaw * RadToDeg, 0.0f);
}

/** UE-like FDirectionalLight: the transform drives the aim; no raw direction field. */
struct ENGINE_API FDirectionalLight
{
	FLegacyTransform Transform{FVector(0.0f, 0.0f, 0.0f), FVector(60.3f, 142.1f, 0.0f), FVector(1.0f, 1.0f, 1.0f)};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f); // linear RGB
	float Intensity = 1.0f;
	bool bCastShadows = true;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	[[nodiscard]] FVector GetDirection() const
	{
		return LightDirectionFromRotation(Transform.RotationDegrees);
	}
};

/** UE-like FPointLight: location from the transform; attenuation Range. */
struct ENGINE_API FPointLight
{
	FLegacyTransform Transform{FVector(0.0f, 2.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f); // linear RGB
	float Intensity = 1.0f;
	float Range = 8.0f;
	bool bCastShadows = false;

	/** Optional orbit animation (level JSON orbit); preserved for the save round trip. */
	bool bHasOrbit = false;
	float OrbitRadius = 1.0f;
	float OrbitHeight = 1.0f;
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;
};
