#pragma once

#include "CoreMinimal.h"
#include "LegacyCoordinateConversion.h"

constexpr int32 MaxDirectionalLights = 2;
constexpr int32 MaxPointLights = 4;

/** UE FDirectionalLight Source Angle default (~sun disc), in degrees. */
constexpr float DefaultLightSourceAngleDegrees = 0.5357f;

/** UE-like FDirectionalLight: the transform drives the aim; no raw direction field. */
struct ENGINE_API FDirectionalLight
{
	/** The default sun: legacy pitch 60.3, yaw 142.1 degrees. */
	FTransform Transform{FLegacyCoordinateConversion::ConvertLightRotation(60.3f, 142.1f)};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f); // linear RGB
	float Intensity = 1.0f;
	bool bCastShadows = true;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	[[nodiscard]] FVector GetDirection() const
	{
		const FVector Direction = Transform.GetRotation().RotateVector(FLegacyCoordinateConversion::LightForward());
		return Direction / Direction.Size();
	}
};

/** UE-like FPointLight: location from the transform; attenuation Range. */
struct ENGINE_API FPointLight
{
	FTransform Transform{FLegacyCoordinateConversion::ConvertPosition(FVector(0.0f, 2.0f, 0.0f))};
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
