#include "Components/DirectionalLightComponent.h"

UDirectionalLightComponent::UDirectionalLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDirectionalLightComponent::SetLightSourceAngle(float NewLightSourceAngle)
{
	LightSourceAngle = NewLightSourceAngle;
	MarkRenderStateDirty();
}
