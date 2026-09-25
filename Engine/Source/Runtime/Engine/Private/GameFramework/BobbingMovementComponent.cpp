#include "GameFramework/BobbingMovementComponent.h"

#include "Components/SceneComponent.h"

UBobbingMovementComponent::UBobbingMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetComponentTickEnabled(true);
}

void UBobbingMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (UpdatedComponent == nullptr)
	{
		return;
	}
	Time += DeltaTime;
	FVector Location = UpdatedComponent->GetComponentLocation();
	Location.Z = BaseZ + (Amplitude * (0.5f + (0.5f * FMath::Sin(Time * Speed))));
	UpdatedComponent->SetWorldLocation(Location);
}
