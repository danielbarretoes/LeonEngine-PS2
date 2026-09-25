#include "GameFramework/FloatingPawnMovement.h"

#include "Components/SceneComponent.h"

UFloatingPawnMovement::UFloatingPawnMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetComponentTickEnabled(true);
}

void UFloatingPawnMovement::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (UpdatedComponent == nullptr)
	{
		return;
	}
	// The input direction, unit length (the legacy fly camera normalized its wish direction the same way).
	FVector Direction = ConsumeInputVector();
	const float Length = Direction.Size();
	if (Length > 1.0e-4f)
	{
		Direction /= Length;
	}
	else
	{
		Direction = FVector::ZeroVector;
	}
	Velocity = Direction * MaxSpeed;
	if (!Velocity.IsZero())
	{
		UpdatedComponent->SetWorldLocation(UpdatedComponent->GetComponentLocation() + Velocity * DeltaTime);
	}
}
