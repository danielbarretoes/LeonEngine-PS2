#include "GameFramework/Actor.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

const FName AActor::DefaultSceneRootName(TEXT("DefaultSceneRoot"));

AActor::AActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = false;
	bCanEverTick = true;
	// UE actors have no root by default; Leon gives every actor one so it always has a transform. A subclass with its
	// own root (ACharacter's capsule) skips it through DoNotCreateDefaultSubobject(DefaultSceneRootName).
	RootComponent = ObjectInitializer.CreateOptionalDefaultSubobject<USceneComponent>(this, DefaultSceneRootName);
}

UWorld* AActor::GetWorld() const
{
	const ULevel* Level = GetLevel();
	return Level != nullptr ? Level->OwningWorld : nullptr;
}

ULevel* AActor::GetLevel() const
{
	return GetTypedOuter<ULevel>();
}

bool AActor::SetRootComponent(USceneComponent* NewRootComponent)
{
	if (NewRootComponent != nullptr && NewRootComponent->GetOwner() != this)
	{
		return false;
	}
	RootComponent = NewRootComponent;
	return true;
}

void AActor::AddOwnedComponent(UActorComponent* Component)
{
	if (Component != nullptr)
	{
		OwnedComponents.AddUnique(Component);
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	OwnedComponents.Remove(Component);
}

void AActor::RegisterAllComponents()
{
	UWorld* World = GetWorld();
	// The root first, so its children find it registered (UE).
	if (RootComponent != nullptr && RootComponent->bAutoRegister)
	{
		RootComponent->RegisterComponentWithWorld(World);
	}
	// Registration may add components (a component creating another): iterate over a copy.
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->bAutoRegister && !Component->IsPendingKill())
		{
			Component->RegisterComponentWithWorld(World);
		}
	}
}

void AActor::UnregisterAllComponents()
{
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsRegistered())
		{
			Component->UnregisterComponent();
		}
	}
}

void AActor::SetActorHiddenInGame(bool bNewHidden)
{
	if (bHidden == bNewHidden)
	{
		return;
	}
	bHidden = bNewHidden;
	// UE: MarkComponentsRenderStateDirty.
	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component != nullptr)
		{
			Component->MarkRenderStateDirty();
		}
	}
}

void AActor::SetOwner(AActor* NewOwner)
{
	Owner = NewOwner;
}

FVector AActor::GetActorLocation() const
{
	return RootComponent != nullptr ? RootComponent->GetComponentLocation() : FVector::ZeroVector;
}

FRotator AActor::GetActorRotation() const
{
	return RootComponent != nullptr ? RootComponent->GetComponentRotation() : FRotator::ZeroRotator;
}

FQuat AActor::GetActorQuat() const
{
	return RootComponent != nullptr ? RootComponent->GetComponentQuat() : FQuat::Identity;
}

FTransform AActor::GetActorTransform() const
{
	return RootComponent != nullptr ? RootComponent->GetComponentTransform() : FTransform::Identity;
}

FVector AActor::GetActorScale3D() const
{
	return RootComponent != nullptr ? RootComponent->GetComponentScale() : FVector::OneVector;
}

FVector AActor::GetActorForwardVector() const
{
	return RootComponent != nullptr ? RootComponent->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
}

FVector AActor::GetActorRightVector() const
{
	return RootComponent != nullptr ? RootComponent->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
}

FVector AActor::GetActorUpVector() const
{
	return RootComponent != nullptr ? RootComponent->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
}

bool AActor::SetActorLocation(const FVector& NewLocation)
{
	if (RootComponent == nullptr)
	{
		return false;
	}
	RootComponent->SetWorldLocation(NewLocation);
	return true;
}

bool AActor::SetActorRotation(const FRotator& NewRotation)
{
	if (RootComponent == nullptr)
	{
		return false;
	}
	RootComponent->SetWorldRotation(NewRotation);
	return true;
}

bool AActor::SetActorLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (RootComponent == nullptr)
	{
		return false;
	}
	RootComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
	return true;
}

bool AActor::SetActorTransform(const FTransform& NewTransform)
{
	if (RootComponent == nullptr)
	{
		return false;
	}
	RootComponent->SetWorldTransform(NewTransform);
	return true;
}

void AActor::SetActorScale3D(const FVector& NewScale3D)
{
	if (RootComponent != nullptr)
	{
		RootComponent->SetRelativeScale3D(NewScale3D);
	}
}

bool AActor::Destroy()
{
	if (bActorIsBeingDestroyed || IsPendingKill())
	{
		return true;
	}
	if (UWorld* World = GetWorld())
	{
		return World->DestroyActor(this);
	}
	// Outside any world (an engine-owned HUD, a test actor): no level to leave.
	bActorIsBeingDestroyed = true;
	Destroyed();
	RouteEndPlay(EEndPlayReason::Destroyed);
	UnregisterAllComponents();
	MarkPendingKill();
	return true;
}

void AActor::Destroyed()
{
}

void AActor::PreInitializeComponents()
{
}

void AActor::InitializeComponents()
{
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsRegistered() && Component->bWantsInitializeComponent &&
			!Component->HasBeenInitialized())
		{
			Component->InitializeComponent();
		}
	}
}

void AActor::PostInitializeComponents()
{
	bActorInitialized = true;
}

void AActor::FinishSpawning(const FTransform& Transform)
{
	if (bActorInitialized)
	{
		return;
	}
	SetActorTransform(Transform);
	if (UWorld* World = GetWorld())
	{
		World->PostActorConstruction(this);
	}
}

void AActor::DispatchBeginPlay()
{
	if (bActorHasBegunPlay || bActorBeginningPlay || IsPendingKillPending())
	{
		return;
	}
	bActorBeginningPlay = true;
	BeginPlay();
	bActorBeginningPlay = false;
	bActorHasBegunPlay = true;
}

void AActor::BeginPlay()
{
	// Components begin before the rest of the actor's BeginPlay (overrides call Super first). A component registered
	// during the loop begins through its registration.
	bActorHasBegunPlay = true;
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsRegistered() && !Component->HasBegunPlay())
		{
			Component->BeginPlay();
		}
	}
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->HasBegunPlay())
		{
			Component->EndPlay(EndPlayReason);
		}
	}
}

void AActor::RouteEndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bHasEndedPlay)
	{
		return;
	}
	bHasEndedPlay = true;
	if (bActorHasBegunPlay)
	{
		EndPlay(EndPlayReason);
	}
	bActorHasBegunPlay = false;
}

void AActor::Tick(float /*DeltaSeconds*/)
{
}

void AActor::TickActor(float DeltaSeconds)
{
	const TArray<UActorComponent*> Components = OwnedComponents;
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsRegistered() && Component->IsComponentTickEnabled() &&
			!Component->IsPendingKill())
		{
			Component->TickComponent(DeltaSeconds);
		}
	}
	if (bCanEverTick)
	{
		Tick(DeltaSeconds);
	}
}

void AActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	AActor* This = CastChecked<AActor>(InThis);
	Collector.AddReferencedObjects(This->OwnedComponents, This);
	Super::AddReferencedObjects(InThis, Collector);
}

void AActor::CalcCamera(float /*DeltaTime*/, FMinimalViewInfo& OutResult)
{
	// Leon's camera component is not placed by its transform: its own eye and view rotation are the view.
	if (const UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
	{
		OutResult.Location = Camera->GetCameraLocation();
		OutResult.Rotation = Camera->GetViewRotation();
		OutResult.FOV = Camera->FieldOfView();
		return;
	}
	GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
}

void AActor::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = GetActorLocation();
	OutRotation = GetActorRotation();
}
