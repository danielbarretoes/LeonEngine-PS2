#include "Engine/PointLight.h"

APointLight::APointLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent0"));
	LightComponent = PointLightComponent;
	RootComponent = PointLightComponent;
}
