#include "Components/BoxComponent.h"

UBoxComponent::UBoxComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
	const FVector Scale = GetComponentScale();
	return FVector(
		BoxExtent.X * FMath::Abs(Scale.X), BoxExtent.Y * FMath::Abs(Scale.Y), BoxExtent.Z * FMath::Abs(Scale.Z));
}

FCollisionShape UBoxComponent::GetCollisionShape(float Inflation) const
{
	return FCollisionShape::MakeBox(GetScaledBoxExtent() + FVector(Inflation, Inflation, Inflation));
}
