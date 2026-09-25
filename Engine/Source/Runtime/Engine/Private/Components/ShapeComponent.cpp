#include "Components/ShapeComponent.h"

UShapeComponent::UShapeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ShapeColor(223, 149, 157, 255)
{
	bDrawOnlyIfSelected = false;
	// Shapes are for collision: they cast no shadow (UE).
	CastShadow = false;
}

float UShapeComponent::GetShapeScale() const
{
	const FVector Scale = GetComponentScale();
	return FMath::Min(FMath::Abs(Scale.X), FMath::Min(FMath::Abs(Scale.Y), FMath::Abs(Scale.Z)));
}
