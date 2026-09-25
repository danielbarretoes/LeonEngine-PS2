#pragma once

#include "Components/LightComponent.h"
#include "CoreMinimal.h"
#include "Level/Light.h"
#include "LocalLightComponent.generated.h"

/** A light with a position and a range (UE: ULocalLightComponent). */
UCLASS(Abstract)
class ENGINE_API ULocalLightComponent : public ULightComponent
{
	GENERATED_BODY()

public:
	ULocalLightComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Distance at which the light's influence ends, cm (UE: AttenuationRadius). */
	UPROPERTY()
	float AttenuationRadius = DefaultPointLightRange;

	/** UE: SetAttenuationRadius; a registered light's proxy is recreated. */
	void SetAttenuationRadius(float NewRadius);
};
