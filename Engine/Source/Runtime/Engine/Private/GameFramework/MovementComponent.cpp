#include "GameFramework/MovementComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UMovementComponent::UMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
}

float UMovementComponent::GetMaxSpeed() const
{
	return 0.0f;
}

void UMovementComponent::OnRegister()
{
	Super::OnRegister();
	if (UpdatedComponent == nullptr)
	{
		if (AActor* Owner = GetOwner())
		{
			SetUpdatedComponent(Owner->GetRootComponent());
		}
	}
}
