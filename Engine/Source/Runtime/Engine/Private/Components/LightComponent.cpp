#include "Components/LightComponent.h"

ULightComponent::ULightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector ULightComponent::GetDirection() const
{
	const FVector Direction = GetComponentTransform().GetRotation().GetForwardVector();
	return Direction / Direction.Size();
}
