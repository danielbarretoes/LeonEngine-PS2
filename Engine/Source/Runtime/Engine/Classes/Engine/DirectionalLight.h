#pragma once

#include "Components/DirectionalLightComponent.h"
#include "CoreMinimal.h"
#include "Engine/Light.h"
#include "DirectionalLight.generated.h"

/**
 * A sun (UE: ADirectionalLight). It starts with Leon's default sun rotation (FDirectionalLight's), and shines along its
 * forward axis.
 */
UCLASS()
class ENGINE_API ADirectionalLight : public ALight
{
	GENERATED_BODY()

public:
	ADirectionalLight(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The root light as its class (UE: GetComponent; LightComponent is the same object). */
	[[nodiscard]] UDirectionalLightComponent* GetDirectionalLightComponent() const
	{
		return DirectionalLightComponent;
	}

private:
	UPROPERTY()
	UDirectionalLightComponent* DirectionalLightComponent = nullptr;
};
