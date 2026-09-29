#include "Components/ActorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UActorComponent::UActorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bWantsInitializeComponent = false;
	// UE: a component does not tick unless its class says so (PrimaryComponentTick.bCanEverTick).
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = true;
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
	RegisterAllComponentTickFunctions(false);
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
		if (OwnerPrivate->HasActorBegunPlay())
		{
			// UE: its tick joins the world's (again, after an unregistration) while its owner plays.
			RegisterAllComponentTickFunctions(true);
			if (!bHasBegunPlay)
			{
				BeginPlay();
			}
		}
	}
}

void UActorComponent::UnregisterComponent()
{
	if (!bRegistered)
	{
		return;
	}
	RegisterAllComponentTickFunctions(false);
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

void UActorComponent::MarkRenderStateDirty()
{
	if (!bRenderStateCreated)
	{
		return;
	}
	DestroyRenderState_Concurrent();
	CreateRenderState_Concurrent();
}

void UActorComponent::SendRenderTransform_Concurrent()
{
}

void UActorComponent::SendRenderDynamicData_Concurrent()
{
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

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
	if (PrimaryComponentTick.bCanEverTick && !IsTemplate())
	{
		PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
	}
}

void UActorComponent::SetComponentTickInterval(float TickInterval)
{
	PrimaryComponentTick.UpdateTickIntervalAndCoolDown(TickInterval);
}

void UActorComponent::RegisterAllComponentTickFunctions(bool bRegister)
{
	RegisterComponentTickFunctions(bRegister);
}

void UActorComponent::RegisterComponentTickFunctions(bool bRegister)
{
	if (bRegister)
	{
		if (PrimaryComponentTick.bCanEverTick && bRegistered && !IsTemplate() && OwnerPrivate != nullptr)
		{
			PrimaryComponentTick.Target = this;
			PrimaryComponentTick.RegisterTickFunction(OwnerPrivate->GetLevel());
		}
	}
	else
	{
		PrimaryComponentTick.UnRegisterTickFunction();
	}
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
