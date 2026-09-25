#include "GameFramework/Actor.h"

#include "Components/ActorComponent.h"

AActor::~AActor()
{
	// Members (root, Character mesh, …) destroy after this body. Clear registry first so
	// component dtors do not touch a destroyed `Components` vector.
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr)
		{
			Component->bRegistered = false;
			Component->Owner = nullptr;
		}
	}
	Components.Empty();
	OwnedComponents.Empty();
}

void AActor::RegisterComponent(UActorComponent* Component)
{
	if (Component == nullptr || Component->bRegistered)
	{
		return;
	}
	Component->SetOwner(this);
	Component->bRegistered = true;
	Components.Add(Component);
	// CreateDefaultSubobject after SpawnActor: match Unreal late-register BeginPlay.
	if (bHasBegunPlay)
	{
		Component->BeginPlay();
	}
}

void AActor::UnregisterComponent(UActorComponent* Component)
{
	if (Component == nullptr)
	{
		return;
	}
	Components.Remove(Component);
	Component->bRegistered = false;
}

void AActor::BeginPlayComponents()
{
	bHasBegunPlay = true;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr)
		{
			Component->BeginPlay();
		}
	}
}

void AActor::EndPlayComponents()
{
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr)
		{
			Component->EndPlay();
		}
	}
	bHasBegunPlay = false;
}

void AActor::TickComponents(float DeltaTime)
{
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsComponentTickEnabled())
		{
			Component->TickComponent(DeltaTime);
		}
	}
}
