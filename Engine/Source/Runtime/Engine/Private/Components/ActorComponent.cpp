#include "Components/ActorComponent.h"

#include "GameFramework/Actor.h"

UActorComponent::~UActorComponent()
{
	DestroyComponent();
}

void UActorComponent::DestroyComponent()
{
	if (bRegistered && Owner != nullptr)
	{
		Owner->UnregisterComponent(this);
	}
	bRegistered = false;
	Owner = nullptr;
}
