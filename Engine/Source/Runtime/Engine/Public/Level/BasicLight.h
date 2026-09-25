#pragma once

#include "CoreMinimal.h"
#include "Level/Light.h"

class ULevel;

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
	FLegacyTransform Transform{};
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f);
	float Intensity = 1.0f;
	bool bCastShadows = true;
	/** Directional: soft-shadow angular diameter (degrees). Ignored for Point today. */
	float SourceAngle = DefaultLightSourceAngleDegrees;
	/** Point: attenuation radius. */
	float Range = 8.0f;

	[[nodiscard]] static FBasicLight Directional(const FVector& RotationDegrees = FVector(60.3f, 142.1f, 0.0f),
		const FVector& InLightColor = FVector(1.0f, 1.0f, 1.0f), float InIntensity = 1.0f);
	[[nodiscard]] static FBasicLight Point(const FVector& Position = FVector(0.0f, 2.0f, 0.0f),
		const FVector& InLightColor = FVector(1.0f, 1.0f, 1.0f), float InIntensity = 1.0f, float InRange = 8.0f);

	[[nodiscard]] FDirectionalLight AsDirectional() const;
	[[nodiscard]] FPointLight AsPoint() const;

	/** Appends this light to the level's directional or point list. */
	void AddTo(ULevel& Level) const;
};

[[nodiscard]] bool TryParseBasicLightName(const FString& Name, EBasicLight& Out);
