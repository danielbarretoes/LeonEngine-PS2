#include "GameFramework/OrbitMovementComponent.h"

#include "Components/SceneComponent.h"

UOrbitMovementComponent::UOrbitMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetComponentTickEnabled(true);
}

void UOrbitMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (UpdatedComponent == nullptr)
	{
		return;
	}
	Time += DeltaTime;
	const float Angle = Time * Speed;
	UpdatedComponent->SetWorldLocation(FVector(
		FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Height + (HeightAmplitude * FMath::Sin(Angle * 2.0f))));
}
