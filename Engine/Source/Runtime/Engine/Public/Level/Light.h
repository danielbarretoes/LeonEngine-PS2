#pragma once

#include "CoreMinimal.h"

constexpr int32 MaxDirectionalLights = 2;
constexpr int32 MaxPointLights = 4;

/** UE FDirectionalLight Source Angle default (~sun disc), in degrees. */
constexpr float DefaultLightSourceAngleDegrees = 0.5357f;

/** Default point light attenuation radius (8 m), in world units (cm). */
constexpr float DefaultPointLightRange = 800.0f;

/** UE-like FDirectionalLight: the transform drives the aim (the light travels along its forward axis, +X). */
struct ENGINE_API FDirectionalLight
{
	/** The default sun (the legacy pitch 60.3, yaw 142.1 degrees): down 60.3 degrees, toward yaw -52.1. */
	FTransform Transform{FRotator(-60.3f, -52.1f, 0.0f)};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f); // linear RGB
	float Intensity = 1.0f;
	bool bCastShadows = true;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	[[nodiscard]] FVector GetDirection() const
	{
		const FVector Direction = Transform.GetRotation().GetForwardVector();
		return Direction / Direction.Size();
	}
};

/** UE-like FPointLight: location from the transform; attenuation Range. */
struct ENGINE_API FPointLight
{
	/** 2 m up. */
	FTransform Transform{FVector(0.0f, 0.0f, 200.0f)};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f); // linear RGB
	float Intensity = 1.0f;
	float Range = DefaultPointLightRange;
	bool bCastShadows = false;

	/** Optional orbit animation (level JSON orbit); preserved for the save round trip. Lengths in cm. */
	bool bHasOrbit = false;
	float OrbitRadius = 100.0f;
	float OrbitHeight = 100.0f;
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;
};
