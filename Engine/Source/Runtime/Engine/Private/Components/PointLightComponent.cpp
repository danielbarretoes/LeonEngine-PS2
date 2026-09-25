#include "Components/PointLightComponent.h"

UPointLightComponent::UPointLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Leon's renderer never shadows point lights; the legacy point lights did not cast.
	CastShadows = false;
}
