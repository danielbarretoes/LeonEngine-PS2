#include "Components/LocalLightComponent.h"

ULocalLightComponent::ULocalLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULocalLightComponent::SetAttenuationRadius(float NewRadius)
{
	AttenuationRadius = NewRadius;
	MarkRenderStateDirty();
}
