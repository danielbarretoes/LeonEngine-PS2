#include "LightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"

FLightSceneProxy::FLightSceneProxy(const ULightComponent* InComponent)
	: LightToWorld(InComponent->GetComponentTransform())
	, Color(InComponent->LightColor.R, InComponent->LightColor.G, InComponent->LightColor.B)
	, Intensity(InComponent->Intensity)
	, bCastShadows(InComponent->CastShadows)
{
	if (const UDirectionalLightComponent* Directional = Cast<UDirectionalLightComponent>(InComponent))
	{
		LightType = ELightSceneProxyType::Directional;
		SourceAngle = Directional->LightSourceAngle;
	}
	else if (const ULocalLightComponent* Local = Cast<ULocalLightComponent>(InComponent))
	{
		LightType = ELightSceneProxyType::Point;
		Radius = Local->AttenuationRadius;
	}
}
