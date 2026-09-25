#include "Components/CapsuleComponent.h"

UCapsuleComponent::UCapsuleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight)
{
	// UE keeps the half height at least the radius (the caps meet in a sphere).
	CapsuleRadius = FMath::Max(0.0f, InRadius);
	CapsuleHalfHeight = FMath::Max(InHalfHeight, CapsuleRadius);
}

FCollisionShape UCapsuleComponent::GetCollisionShape(float Inflation) const
{
	return FCollisionShape::MakeCapsule(GetScaledCapsuleRadius() + Inflation, GetScaledCapsuleHalfHeight() + Inflation);
}
