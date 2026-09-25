#pragma once

#include "Components/LightComponent.h"
#include "CoreMinimal.h"
#include "Level/Light.h"
#include "DirectionalLightComponent.generated.h"

/** A light infinitely far away, like the sun (UE: UDirectionalLightComponent); only its rotation matters. */
UCLASS()
class ENGINE_API UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()

public:
	UDirectionalLightComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Angular diameter of the light source, degrees; widens the shadow filter (UE: LightSourceAngle). */
	UPROPERTY()
	float LightSourceAngle = DefaultLightSourceAngleDegrees;

	/** UE: SetLightSourceAngle; a registered light's proxy is recreated. */
	void SetLightSourceAngle(float NewLightSourceAngle);
};
