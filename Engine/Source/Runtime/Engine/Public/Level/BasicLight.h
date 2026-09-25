#pragma once

#include "CoreMinimal.h"
#include "Level/Light.h"

class ALight;
class UWorld;

/** Engine light primitives (UE-like FDirectionalLight / FPointLight). */
enum class EBasicLight
{
	Directional,
	Point,
};

/** Placeable light: transform + UE Details fields (Intensity, LightColor, bCastShadows, ...). */
struct ENGINE_API FBasicLight
{
	EBasicLight Type = EBasicLight::Directional;
	FTransform Transform;
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f);
	float Intensity = 1.0f;
	bool bCastShadows = true;
	/** Directional: soft-shadow angular diameter (degrees). Ignored for Point today. */
	float SourceAngle = DefaultLightSourceAngleDegrees;
	/** Point: attenuation radius (cm). */
	float Range = DefaultPointLightRange;

	/** A directional light; the default rotation is the default sun (FDirectionalLight). */
	[[nodiscard]] static FBasicLight Directional(const FQuat& Rotation = FDirectionalLight().Transform.GetRotation(),
		const FVector& InLightColor = FVector(1.0f, 1.0f, 1.0f), float InIntensity = 1.0f);
	[[nodiscard]] static FBasicLight Point(const FVector& Location = FPointLight().Transform.GetLocation(),
		const FVector& InLightColor = FVector(1.0f, 1.0f, 1.0f), float InIntensity = 1.0f,
		float InRange = DefaultPointLightRange);

	[[nodiscard]] FDirectionalLight AsDirectional() const;
	[[nodiscard]] FPointLight AsPoint() const;

	/** Spawns an ADirectionalLight or APointLight at Transform with these settings. */
	ALight* SpawnIn(UWorld& World) const;
};

[[nodiscard]] bool TryParseBasicLightName(const FString& Name, EBasicLight& Out);
