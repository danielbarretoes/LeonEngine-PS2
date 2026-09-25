#pragma once

#include "CoreMinimal.h"

class ULightComponent;

/** Which light a FLightSceneProxy is. */
enum class ELightSceneProxyType : uint8
{
	Directional,
	Point,
};

/**
 * What the renderer knows of a light component (UE: FLightSceneProxy): a snapshot taken by
 * ULightComponent::CreateSceneProxy, plus the world transform the world sends again before each frame
 * (FSceneInterface::UpdateLightTransform). The renderer lights the first MaxDirectionalLights directional and
 * MaxPointLights point lights of the scene and shadows the first directional one when it casts shadows.
 */
class ENGINE_API FLightSceneProxy
{
public:
	explicit FLightSceneProxy(const ULightComponent* InComponent);

	[[nodiscard]] ELightSceneProxyType GetLightType() const
	{
		return LightType;
	}

	/** UE: SetTransform. */
	void SetTransform(const FTransform& InLightToWorld)
	{
		LightToWorld = InLightToWorld;
	}
	[[nodiscard]] const FTransform& GetLightToWorld() const
	{
		return LightToWorld;
	}

	/** The unit direction the light travels: its forward axis (UE: GetDirection). */
	[[nodiscard]] FVector GetDirection() const
	{
		const FVector Direction = LightToWorld.GetRotation().GetForwardVector();
		return Direction / Direction.Size();
	}
	/** UE: GetPosition (the location; UE's directional lights return a direction there). */
	[[nodiscard]] FVector GetPosition() const
	{
		return LightToWorld.GetLocation();
	}

	/** Linear colour times the intensity (UE: GetColor). */
	[[nodiscard]] FVector GetColor() const
	{
		return Color * Intensity;
	}
	[[nodiscard]] bool CastsDynamicShadow() const
	{
		return bCastShadows;
	}
	/** A directional light's source angle, degrees (UE: LightSourceAngle). */
	[[nodiscard]] float GetSourceAngle() const
	{
		return SourceAngle;
	}
	/** A point light's attenuation radius, cm (UE: GetRadius). */
	[[nodiscard]] float GetRadius() const
	{
		return Radius;
	}

private:
	FTransform LightToWorld;
	FVector Color = FVector(1.0f, 1.0f, 1.0f);
	float Intensity = 1.0f;
	float SourceAngle = 0.0f;
	float Radius = 0.0f;
	ELightSceneProxyType LightType = ELightSceneProxyType::Directional;
	bool bCastShadows = false;
};
