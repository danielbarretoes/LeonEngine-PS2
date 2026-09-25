#include "GameFramework/RotatingMovementComponent.h"

#include "Components/SceneComponent.h"

URotatingMovementComponent::URotatingMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetComponentTickEnabled(true);
}

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (UpdatedComponent == nullptr || DeltaTime <= 0.0f)
	{
		return;
	}
	// UE: URotatingMovementComponent::TickComponent.
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
	const FQuat DeltaRotation = (RotationRate * DeltaTime).Quaternion();
	const FQuat NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);
	FVector DeltaLocation = FVector::ZeroVector;
	if (!PivotTranslation.IsZero())
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = OldPivot - NewPivot;
	}
	UpdatedComponent->SetWorldLocationAndRotation(
		UpdatedComponent->GetComponentLocation() + DeltaLocation, NewRotation.Rotator());
}
