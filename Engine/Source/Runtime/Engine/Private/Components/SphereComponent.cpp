#include "Components/SphereComponent.h"

USphereComponent::USphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FCollisionShape USphereComponent::GetCollisionShape(float Inflation) const
{
	return FCollisionShape::MakeSphere(GetScaledSphereRadius() + Inflation);
}
