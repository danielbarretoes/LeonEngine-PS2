#include "Engine/DirectionalLight.h"

ADirectionalLight::ADirectionalLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("LightComponent0"));
	DirectionalLightComponent->SetRelativeTransform(FDirectionalLight().Transform);
	LightComponent = DirectionalLightComponent;
	RootComponent = DirectionalLightComponent;
}
