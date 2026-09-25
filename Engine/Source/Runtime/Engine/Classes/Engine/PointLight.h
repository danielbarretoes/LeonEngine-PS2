#pragma once

#include "Components/PointLightComponent.h"
#include "CoreMinimal.h"
#include "Engine/Light.h"
#include "PointLight.generated.h"

/** A light bulb (UE: APointLight): shines from its location up to the attenuation radius. */
UCLASS()
class ENGINE_API APointLight : public ALight
{
	GENERATED_BODY()

public:
	APointLight(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: PointLightComponent (the same object as LightComponent). */
	[[nodiscard]] UPointLightComponent* GetPointLightComponent() const
	{
		return PointLightComponent;
	}

private:
	UPROPERTY()
	UPointLightComponent* PointLightComponent = nullptr;
};
