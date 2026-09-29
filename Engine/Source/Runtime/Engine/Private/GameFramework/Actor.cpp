#include "GameFramework/Actor.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Stats/Stats.h"
#include "TimerManager.h"

DECLARE_CYCLE_STAT(TEXT("Register Components"), STAT_RegisterAllComponents, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Begin Play"), STAT_ActorBeginPlay, STATGROUP_Engine);

namespace
{
	/**
	 * A copy of an actor's components to iterate while they may add or remove others (registration, a tick spawning a
	 * component): inline storage for the usual handful, so the per-frame TickActor allocates nothing.
	 */
	using FComponentSnapshot = TArray<UActorComponent*, TInlineAllocator<16>>;
} // namespace

const FName AActor::DefaultSceneRootName(TEXT("DefaultSceneRoot"));

AActor::AActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = false;
	bCanBeDamaged = true;
	// UE: an actor does not tick unless its class says so (PrimaryActorTick.bCanEverTick), in the PrePhysics group.
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = true;
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
	SCOPE_CYCLE_COUNTER(STAT_RegisterAllComponents);
	UWorld* World = GetWorld();
	// The root first, so its children find it registered (UE).
	if (RootComponent != nullptr && RootComponent->bAutoRegister)
	{
		RootComponent->RegisterComponentWithWorld(World);
	}
	// Registration may add components (a component creating another): iterate over a copy.
	const FComponentSnapshot Components(OwnedComponents);
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
	const FComponentSnapshot Components(OwnedComponents);
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
	const FComponentSnapshot Components(OwnedComponents);
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
	{
		SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
		BeginPlay();
	}
	bActorBeginningPlay = false;
	bActorHasBegunPlay = true;
}

void AActor::BeginPlay()
{
	// Components begin before the rest of the actor's BeginPlay (overrides call Super first). A component registered
	// during the loop begins through its registration.
	bActorHasBegunPlay = true;
	// UE: the initial life span starts with play.
	if (InitialLifeSpan > 0.0f)
	{
		SetLifeSpan(InitialLifeSpan);
	}
	// UE: the actor's tick function, then each component's before it begins play.
	RegisterAllActorTickFunctions(true, false);
	const FComponentSnapshot Components(OwnedComponents);
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->IsRegistered() && !Component->HasBegunPlay())
		{
			Component->RegisterAllComponentTickFunctions(true);
			Component->BeginPlay();
		}
	}
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const FComponentSnapshot Components(OwnedComponents);
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
	// UE: nothing of the actor ticks or waits on a timer once it stops playing.
	RegisterAllActorTickFunctions(false, true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void AActor::Tick(float /*DeltaSeconds*/)
{
}

void AActor::TickActor(float DeltaSeconds, ELevelTick /*TickType*/, FActorTickFunction& /*ThisTickFunction*/)
{
	Tick(DeltaSeconds);
}

void AActor::RegisterActorTickFunctions(bool bRegister)
{
	if (bRegister)
	{
		if (PrimaryActorTick.bCanEverTick)
		{
			PrimaryActorTick.Target = this;
			PrimaryActorTick.RegisterTickFunction(GetLevel());
		}
	}
	else
	{
		PrimaryActorTick.UnRegisterTickFunction();
	}
}

void AActor::RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents)
{
	RegisterActorTickFunctions(bRegister);
	if (!bDoComponents)
	{
		return;
	}
	const FComponentSnapshot Components(OwnedComponents);
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr)
		{
			Component->RegisterAllComponentTickFunctions(bRegister);
		}
	}
}

void AActor::SetActorTickEnabled(bool bEnabled)
{
	if (PrimaryActorTick.bCanEverTick && !IsTemplate())
	{
		PrimaryActorTick.SetTickFunctionEnable(bEnabled);
	}
}

void AActor::SetActorTickInterval(float TickInterval)
{
	PrimaryActorTick.UpdateTickIntervalAndCoolDown(TickInterval);
}

void AActor::AddTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrerequisiteActor != nullptr && PrerequisiteActor != this)
	{
		PrimaryActorTick.AddPrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void AActor::RemoveTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrerequisiteActor != nullptr)
	{
		PrimaryActorTick.RemovePrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void AActor::AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrerequisiteComponent != nullptr)
	{
		PrimaryActorTick.AddPrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void AActor::RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrerequisiteComponent != nullptr)
	{
		PrimaryActorTick.RemovePrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

FTimerManager& AActor::GetWorldTimerManager() const
{
	UWorld* World = GetWorld();
	check(World != nullptr);
	return World->GetTimerManager();
}

void AActor::BeginDestroy()
{
	// An actor collected without ending play (a world torn down around it) leaves its world's tick lists.
	RegisterActorTickFunctions(false);
	Super::BeginDestroy();
}

namespace
{

	/** Who hears noises (AActor::SetMakeNoiseDelegate). */
	FMakeNoiseDelegate& GetMakeNoiseDelegate()
	{
		static FMakeNoiseDelegate Delegate;
		return Delegate;
	}

} // namespace

void AActor::SetMakeNoiseDelegate(const FMakeNoiseDelegate& NewDelegate)
{
	GetMakeNoiseDelegate() = NewDelegate;
}

void AActor::MakeNoise(float Loudness, APawn* NoiseInstigator, FVector NoiseLocation)
{
	if (NoiseInstigator == nullptr)
	{
		NoiseInstigator = GetInstigator() != nullptr ? GetInstigator() : Cast<APawn>(this);
	}
	if (NoiseLocation.IsZero())
	{
		NoiseLocation = GetActorLocation();
	}
	if (GetMakeNoiseDelegate())
	{
		GetMakeNoiseDelegate()(this, Loudness, NoiseInstigator, NoiseLocation);
	}
}

void AActor::SetLifeSpan(float InLifespan)
{
	// UE: a timer of the world's timer manager; 0 cancels it.
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (InLifespan > 0.0f)
	{
		World->GetTimerManager().SetTimer(TimerHandle_LifeSpanExpired, this, &AActor::LifeSpanExpired, InLifespan);
	}
	else
	{
		World->GetTimerManager().ClearTimer(TimerHandle_LifeSpanExpired);
	}
}

float AActor::GetLifeSpan() const
{
	const UWorld* World = GetWorld();
	const float Remaining =
		World != nullptr ? World->GetTimerManager().GetTimerRemaining(TimerHandle_LifeSpanExpired) : -1.0f;
	return Remaining >= 0.0f ? Remaining : 0.0f;
}

void AActor::LifeSpanExpired()
{
	Destroy();
}

float AActor::TakeDamage(
	float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!bCanBeDamaged)
	{
		return 0.0f;
	}
	const UDamageType* const DamageTypeCDO = DamageEvent.DamageTypeClass != nullptr
		? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>()
		: GetDefault<UDamageType>();
	float ActualDamage = DamageAmount;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		ActualDamage = InternalTakePointDamage(ActualDamage, PointDamageEvent, EventInstigator, DamageCauser);
		if (ActualDamage != 0.0f)
		{
			const FHitResult& Hit = PointDamageEvent.HitInfo;
			OnTakePointDamage.Broadcast(this, ActualDamage, EventInstigator, Hit.ImpactPoint, Hit.GetComponent(),
				NAME_None, PointDamageEvent.ShotDirection, DamageTypeCDO, DamageCauser);
		}
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent& RadialDamageEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);
		ActualDamage = InternalTakeRadialDamage(ActualDamage, RadialDamageEvent, EventInstigator, DamageCauser);
		if (ActualDamage != 0.0f)
		{
			const FHitResult Hit =
				RadialDamageEvent.ComponentHits.Num() > 0 ? RadialDamageEvent.ComponentHits[0] : FHitResult();
			OnTakeRadialDamage.Broadcast(
				this, ActualDamage, DamageTypeCDO, RadialDamageEvent.Origin, Hit, EventInstigator, DamageCauser);
		}
	}
	if (ActualDamage != 0.0f)
	{
		OnTakeAnyDamage.Broadcast(this, ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
	}
	return ActualDamage;
}

float AActor::InternalTakePointDamage(float Damage, const FPointDamageEvent& /*PointDamageEvent*/,
	AController* /*EventInstigator*/, AActor* /*DamageCauser*/)
{
	return Damage;
}

float AActor::InternalTakeRadialDamage(float Damage, const FRadialDamageEvent& RadialDamageEvent,
	AController* /*EventInstigator*/, AActor* /*DamageCauser*/)
{
	// UE: the falloff at the component hit closest to the origin.
	float ClosestHitDistSq = TNumericLimits<float>::Max();
	for (const FHitResult& Hit : RadialDamageEvent.ComponentHits)
	{
		ClosestHitDistSq = FMath::Min(ClosestHitDistSq, (Hit.ImpactPoint - RadialDamageEvent.Origin).SizeSquared());
	}
	const float RadialDamageScale = RadialDamageEvent.ComponentHits.Num() > 0
		? RadialDamageEvent.Params.GetDamageScale(FMath::Sqrt(ClosestHitDistSq))
		: 0.0f;
	return FMath::Lerp(RadialDamageEvent.Params.MinimumDamage, Damage, FMath::Max(0.0f, RadialDamageScale));
}

void AActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	AActor* This = CastChecked<AActor>(InThis);
	Collector.AddReferencedObjects(This->OwnedComponents, This);
	Super::AddReferencedObjects(InThis, Collector);
}

void AActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	// Leon's camera component is not placed by its transform (unless it follows the pawn's control rotation): its
	// own eye and view rotation are the view (UE: UCameraComponent::GetCameraView).
	if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
	{
		Camera->GetCameraView(DeltaTime, OutResult);
		return;
	}
	GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
}

void AActor::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = GetActorLocation();
	OutRotation = GetActorRotation();
}
