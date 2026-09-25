#include "Components/ActorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UActorComponent::UActorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bWantsInitializeComponent = false;
}

void UActorComponent::PostInitProperties()
{
	Super::PostInitProperties();
	// UE: the owner is the actor outer; it keeps its components in OwnedComponents.
	OwnerPrivate = GetTypedOuter<AActor>();
	if (OwnerPrivate != nullptr)
	{
		OwnerPrivate->AddOwnedComponent(this);
	}
}

void UActorComponent::BeginDestroy()
{
	// The collector frees the component: whatever it still registered goes away first.
	if (bRegistered)
	{
		UnregisterComponent();
	}
	Super::BeginDestroy();
}

UWorld* UActorComponent::GetWorld() const
{
	if (WorldPrivate != nullptr)
	{
		return WorldPrivate;
	}
	return OwnerPrivate != nullptr ? OwnerPrivate->GetWorld() : nullptr;
}

void UActorComponent::RegisterComponent()
{
	RegisterComponentWithWorld(OwnerPrivate != nullptr ? OwnerPrivate->GetWorld() : nullptr);
}

void UActorComponent::RegisterComponentWithWorld(UWorld* InWorld)
{
	if (bRegistered || IsPendingKill())
	{
		return;
	}
	WorldPrivate = InWorld;
	bRegistered = true;
	OnRegister();
	if (InWorld != nullptr)
	{
		CreateRenderState_Concurrent();
		CreatePhysicsState();
	}

	// Registered after the owner spawned (UE: a component added at runtime catches up with its actor).
	if (OwnerPrivate != nullptr)
	{
		if (bWantsInitializeComponent && !bHasBeenInitialized && OwnerPrivate->IsActorInitialized())
		{
			InitializeComponent();
		}
		if (!bHasBegunPlay && OwnerPrivate->HasActorBegunPlay())
		{
			BeginPlay();
		}
	}
}

void UActorComponent::UnregisterComponent()
{
	if (!bRegistered)
	{
		return;
	}
	if (bPhysicsStateCreated)
	{
		DestroyPhysicsState();
	}
	if (bRenderStateCreated)
	{
		DestroyRenderState_Concurrent();
	}
	OnUnregister();
	bRegistered = false;
	WorldPrivate = nullptr;
}

void UActorComponent::RecreatePhysicsState()
{
	if (bPhysicsStateCreated)
	{
		DestroyPhysicsState();
	}
	if (bRegistered && GetWorld() != nullptr)
	{
		CreatePhysicsState();
	}
}

void UActorComponent::DestroyComponent(bool /*bPromoteChildren*/)
{
	if (bIsBeingDestroyed)
	{
		return;
	}
	bIsBeingDestroyed = true;
	if (bHasBegunPlay)
	{
		EndPlay(EEndPlayReason::Destroyed);
	}
	if (bRegistered)
	{
		UnregisterComponent();
	}
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}
	if (OwnerPrivate != nullptr)
	{
		OwnerPrivate->RemoveOwnedComponent(this);
	}
	MarkPendingKill();
}

void UActorComponent::InitializeComponent()
{
	bHasBeenInitialized = true;
}

void UActorComponent::UninitializeComponent()
{
	bHasBeenInitialized = false;
}

void UActorComponent::BeginPlay()
{
	bHasBegunPlay = true;
}

void UActorComponent::EndPlay(const EEndPlayReason::Type /*EndPlayReason*/)
{
	bHasBegunPlay = false;
}

void UActorComponent::TickComponent(float /*DeltaTime*/)
{
}

void UActorComponent::OnRegister()
{
}

void UActorComponent::OnUnregister()
{
}

void UActorComponent::CreateRenderState_Concurrent()
{
	bRenderStateCreated = true;
}

void UActorComponent::DestroyRenderState_Concurrent()
{
	bRenderStateCreated = false;
}

void UActorComponent::CreatePhysicsState()
{
	bPhysicsStateCreated = true;
}

void UActorComponent::DestroyPhysicsState()
{
	bPhysicsStateCreated = false;
}
