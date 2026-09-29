#pragma once

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/Level.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "Actor.generated.h"

class AController;
class FTimerManager;
class APawn;
class UDamageType;
class UInputComponent;
class UPrimitiveComponent;
class UWorld;
struct FDamageEvent;
struct FHitResult;
struct FMinimalViewInfo;
struct FPointDamageEvent;
struct FRadialDamageEvent;

/**
 * The damage delegates of an actor (UE: FTakeAnyDamageSignature, FTakePointDamageSignature,
 * FTakeRadialDamageSignature, dynamic multicast delegates there; Leon's are native multicast delegates with UE's
 * parameters).
 */
using FTakeAnyDamageSignature = TMulticastDelegate<void(AActor* /*DamagedActor*/, float /*Damage*/,
	const UDamageType* /*DamageType*/, AController* /*InstigatedBy*/, AActor* /*DamageCauser*/)>;
using FTakePointDamageSignature = TMulticastDelegate<void(AActor* /*DamagedActor*/, float /*Damage*/,
	AController* /*InstigatedBy*/, FVector /*HitLocation*/, UPrimitiveComponent* /*FHitComponent*/, FName /*BoneName*/,
	FVector /*ShotFromDirection*/, const UDamageType* /*DamageType*/, AActor* /*DamageCauser*/)>;
/**
 * Who hears a noise (UE: FMakeNoiseDelegate): AActor::MakeNoise calls it; the AI module sets it
 * (UPawnSensingComponent's hearing).
 */
using FMakeNoiseDelegate = TFunction<void(
	AActor* /*NoiseMaker*/, float /*Loudness*/, APawn* /*NoiseInstigator*/, const FVector& /*NoiseLocation*/)>;
using FTakeRadialDamageSignature =
	TMulticastDelegate<void(AActor* /*DamagedActor*/, float /*Damage*/, const UDamageType* /*DamageType*/,
		FVector /*Origin*/, const FHitResult& /*HitInfo*/, AController* /*InstigatedBy*/, AActor* /*DamageCauser*/)>;

/**
 * An object placed in a world (UE: AActor).
 *
 * ## Lifetime
 * Spawned with UWorld::SpawnActor, which creates it with NewObject in the world's level (its outer) and adds it to
 * ULevel::Actors; the level keeps it alive. Destroy (UWorld::DestroyActor) ends play, unregisters the components,
 * removes the actor from the level and marks it pending kill: the next garbage collection frees it and clears the
 * UPROPERTY references to it.
 *
 * ## Transform
 * The actor transform is its root component's (UE: GetActorLocation is RootComponent->GetComponentLocation). Every
 * actor has a root: a USceneComponent named DefaultSceneRoot, unless a subclass replaces it (ACharacter's capsule
 * skips it with DoNotCreateDefaultSubobject). Rotations are UE rotations: yaw about Z, 0 faces +X, 90 faces +Y.
 *
 * ## Components
 * Components are default subobjects (CreateDefaultSubobject in a constructor) or NewObject<T>(Actor) followed by
 * RegisterComponent. The actor keeps them in OwnedComponents; SpawnActor registers them (RegisterAllComponents).
 *
 * ## Damage
 * TakeDamage (UE's signature) receives a hit: UGameplayStatics::ApplyDamage / ApplyPointDamage / ApplyRadialDamage call
 * it with an FDamageEvent. The base scales radial damage by its falloff and broadcasts OnTakeAnyDamage and
 * OnTakePointDamage / OnTakeRadialDamage; a game's pawn overrides it to apply the damage to its health. An actor with
 * bCanBeDamaged false takes none.
 *
 * ## Tick
 * PrimaryActorTick (UE) is the actor's tick function: a class that overrides Tick sets PrimaryActorTick.bCanEverTick
 * in its constructor (off by default, as in UE: an actor that does not tick costs the world nothing), and may set its
 * TickGroup and TickInterval. BeginPlay registers it with the world and EndPlay takes it out; SetActorTickEnabled turns
 * it on and off. The components tick through their own tick functions (UActorComponent::PrimaryComponentTick), just
 * before their actor.
 *
 * ## Life span
 * SetLifeSpan (or InitialLifeSpan, applied at BeginPlay) destroys the actor after that many seconds of world steps:
 * a timer of the world's timer manager (UE: TimerHandle_LifeSpanExpired).
 */
UCLASS()
class ENGINE_API AActor : public UObject
{
	GENERATED_BODY()

public:
	AActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Name of the default root component; subclasses with another root skip it (DoNotCreateDefaultSubobject). */
	static const FName DefaultSceneRootName;

	/** Tags for gameplay code (UE: Tags); map importers put gameplay meaning here (plan decision D15). */
	UPROPERTY()
	TArray<FName> Tags;

	/** Hidden in game (UE: bHidden). */
	UPROPERTY()
	uint8 bHidden : 1;

	/** Whether the actor takes damage at all (UE: bCanBeDamaged); TakeDamage and the Apply*Damage helpers check it. */
	UPROPERTY()
	uint8 bCanBeDamaged : 1;

	/** Seconds the actor lives after BeginPlay; 0 lives until destroyed (UE: InitialLifeSpan). */
	UPROPERTY()
	float InitialLifeSpan = 0.0f;

	/** Any damage taken (UE: OnTakeAnyDamage), after OnTakePointDamage / OnTakeRadialDamage. */
	FTakeAnyDamageSignature OnTakeAnyDamage;
	/** Point damage taken (UE: OnTakePointDamage). */
	FTakePointDamageSignature OnTakePointDamage;
	/** Radial damage taken (UE: OnTakeRadialDamage). */
	FTakeRadialDamageSignature OnTakeRadialDamage;

	/** The world of the actor's level, or null for an actor outside any world (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const;
	/** The level the actor was spawned in (UE: GetLevel). */
	[[nodiscard]] ULevel* GetLevel() const;

	/** The root component, whose transform is the actor's (UE: GetRootComponent). */
	[[nodiscard]] USceneComponent* GetRootComponent() const
	{
		return RootComponent;
	}
	/** Replaces the root component; it must be owned by this actor (UE: SetRootComponent). */
	bool SetRootComponent(USceneComponent* NewRootComponent);

	/** Every component the actor owns, in creation order (UE: GetComponents). */
	[[nodiscard]] const TArray<UActorComponent*>& GetComponents() const
	{
		return OwnedComponents;
	}
	/** The first owned component of class T (UE: FindComponentByClass). */
	template <class T>
	[[nodiscard]] T* FindComponentByClass() const
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (T* Typed = Cast<T>(Component))
			{
				return Typed;
			}
		}
		return nullptr;
	}
	/** Every owned component of class T (UE: GetComponents<T>). */
	template <class T, class AllocatorType>
	void GetComponents(TArray<T*, AllocatorType>& OutComponents) const
	{
		OutComponents.Reset();
		for (UActorComponent* Component : OwnedComponents)
		{
			if (T* Typed = Cast<T>(Component))
			{
				OutComponents.Add(Typed);
			}
		}
	}

	/** Called by UActorComponent::PostInitProperties / DestroyComponent (UE). */
	void AddOwnedComponent(UActorComponent* Component);
	void RemoveOwnedComponent(UActorComponent* Component);

	/** Registers every auto-register component, the root first (UE: RegisterAllComponents). */
	void RegisterAllComponents();
	/** Unregisters every registered component (UE: UnregisterAllComponents). */
	void UnregisterAllComponents();

	/** The actor that owns this one (UE: Owner): a player state's controller, a weapon's pawn. */
	[[nodiscard]] AActor* GetOwner() const
	{
		return Owner;
	}
	virtual void SetOwner(AActor* NewOwner);

	/** The pawn responsible for this actor (UE: GetInstigator). */
	[[nodiscard]] APawn* GetInstigator() const
	{
		return Instigator;
	}
	void SetInstigator(APawn* InInstigator)
	{
		Instigator = InInstigator;
	}

	/** UE: ActorHasTag. */
	[[nodiscard]] bool ActorHasTag(FName Tag) const
	{
		return Tags.Contains(Tag);
	}

	/** Spawn-order serial assigned by UWorld::SpawnActor (a deterministic tie-break between actors). */
	void SetUniqueID(uint64 Id)
	{
		UniqueID = Id;
	}
	[[nodiscard]] uint64 GetUniqueID() const
	{
		return UniqueID;
	}
	/**
	 * The actor's place in its level's Actors, counted as actors join it (0 before): an actor spawned while the world
	 * ticks joins when the tick ends, so it may come after actors spawned later. Its tick functions tick in this order.
	 */
	[[nodiscard]] uint64 GetLevelOrder() const
	{
		return LevelOrder;
	}

	/** The root component's world location (UE: GetActorLocation). */
	[[nodiscard]] FVector GetActorLocation() const;
	/** The root component's world rotation (UE: GetActorRotation). */
	[[nodiscard]] FRotator GetActorRotation() const;
	[[nodiscard]] FQuat GetActorQuat() const;
	[[nodiscard]] FTransform GetActorTransform() const;
	[[nodiscard]] FVector GetActorScale3D() const;
	[[nodiscard]] FVector GetActorForwardVector() const;
	[[nodiscard]] FVector GetActorRightVector() const;
	[[nodiscard]] FVector GetActorUpVector() const;

	/** Moves the root component (UE: SetActorLocation without sweep or teleport). Returns false without a root. */
	bool SetActorLocation(const FVector& NewLocation);
	bool SetActorRotation(const FRotator& NewRotation);
	bool SetActorLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
	bool SetActorTransform(const FTransform& NewTransform);
	void SetActorScale3D(const FVector& NewScale3D);

	/** UE: SetActorHiddenInGame / IsHidden; a change recreates the components' render state (their proxies). */
	virtual void SetActorHiddenInGame(bool bNewHidden);
	[[nodiscard]] bool IsHidden() const
	{
		return bHidden;
	}

	/** True once Destroy started or the actor is pending kill (UE: IsPendingKillPending). */
	[[nodiscard]] bool IsPendingKillPending() const
	{
		return bActorIsBeingDestroyed || IsPendingKill();
	}
	[[nodiscard]] bool IsActorBeingDestroyed() const
	{
		return bActorIsBeingDestroyed;
	}

	/**
	 * Destroys the actor through its world (UE: Destroy → UWorld::DestroyActor): Destroyed, EndPlay, components
	 * unregistered, removed from the level, marked pending kill. An actor outside any world is only ended and marked.
	 * Returns true when the actor is (being) destroyed.
	 */
	bool Destroy();

	/** Called when the actor is explicitly destroyed, before EndPlay (UE: Destroyed). Overrides call Super. */
	virtual void Destroyed();

	/**
	 * Spawn sequence (UE): components registered, then PreInitializeComponents, InitializeComponents,
	 * PostInitializeComponents, then DispatchBeginPlay when the world plays.
	 */
	virtual void PreInitializeComponents();
	void InitializeComponents();
	virtual void PostInitializeComponents();
	/** Finishes a spawn with bDeferConstruction (UE: FinishSpawning). */
	void FinishSpawning(const FTransform& Transform);

	/** Begins play once (UE: DispatchBeginPlay → BeginPlay). */
	void DispatchBeginPlay();
	/** Begins play on the registered components; overrides call Super::BeginPlay() first (UE). */
	virtual void BeginPlay();
	/** Ends play on the components; overrides call Super::EndPlay(EndPlayReason) last (UE). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	/** Ends play once, from UWorld::DestroyActor and the world teardown (UE: RouteEndPlay). */
	void RouteEndPlay(const EEndPlayReason::Type EndPlayReason);

	/**
	 * The actor's tick function (UE: PrimaryActorTick). Set bCanEverTick (and TickGroup, TickInterval,
	 * bStartWithTickEnabled) in the constructor; BeginPlay registers it.
	 */
	FActorTickFunction PrimaryActorTick;

	/** Per-step update (UE: Tick); runs only with PrimaryActorTick.bCanEverTick. */
	virtual void Tick(float DeltaSeconds);
	/** What the tick function runs: Tick (UE: TickActor). */
	virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction);

	/** Turns the actor's tick on or off (UE: SetActorTickEnabled); its components keep theirs. */
	void SetActorTickEnabled(bool bEnabled);
	[[nodiscard]] bool IsActorTickEnabled() const
	{
		return PrimaryActorTick.IsTickFunctionEnabled();
	}
	/** Seconds between the actor's ticks, 0 every step (UE: SetActorTickInterval / GetActorTickInterval). */
	void SetActorTickInterval(float TickInterval);
	[[nodiscard]] float GetActorTickInterval() const
	{
		return PrimaryActorTick.TickInterval;
	}
	/** The actor ticks after PrerequisiteActor in their group (UE: AddTickPrerequisiteActor). */
	void AddTickPrerequisiteActor(AActor* PrerequisiteActor);
	void RemoveTickPrerequisiteActor(AActor* PrerequisiteActor);
	/** The actor ticks after PrerequisiteComponent in their group (UE: AddTickPrerequisiteComponent). */
	void AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);
	void RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Registers or unregisters the actor's tick function, and with bDoComponents its components' (UE). */
	void RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents);

	/** The world's timer manager (UE: GetWorldTimerManager); the actor must be in a world. */
	[[nodiscard]] FTimerManager& GetWorldTimerManager() const;

	/** The next place of a component's tick among this actor's (FActorComponentTickFunction's tick order). */
	[[nodiscard]] uint64 NextComponentTickSubOrder()
	{
		return ++NumComponentTickOrders;
	}

	[[nodiscard]] bool HasActorBegunPlay() const
	{
		return bActorHasBegunPlay;
	}
	[[nodiscard]] bool IsActorInitialized() const
	{
		return bActorInitialized;
	}

	/** Reports OwnedComponents to the collector (UE). */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * The view of a player camera manager that views this actor (UE: CalcCamera): its first camera component's view,
	 * else its eyes (GetActorEyesViewPoint).
	 */
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult);

	/** Where the actor looks from: its location and rotation (UE: GetActorEyesViewPoint). */
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 * Receives damage and returns the amount taken (UE: TakeDamage). The base takes nothing when bCanBeDamaged is
	 * false; else a point event goes through InternalTakePointDamage and a radial one through InternalTakeRadialDamage
	 * (the falloff for the closest component hit), then, for a non-zero amount, OnTakePointDamage / OnTakeRadialDamage
	 * and OnTakeAnyDamage are broadcast. Overrides call Super first and apply what it returns (UE ShooterGame).
	 */
	virtual float TakeDamage(
		float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/** UE: CanBeDamaged / SetCanBeDamaged. */
	[[nodiscard]] bool CanBeDamaged() const
	{
		return bCanBeDamaged;
	}
	void SetCanBeDamaged(bool bInCanBeDamaged)
	{
		bCanBeDamaged = bInCanBeDamaged;
	}

	/**
	 * Makes a noise the AI can hear (UE: MakeNoise): Loudness scales how far it carries, NoiseInstigator is the pawn
	 * credited (this actor's instigator, or itself when a pawn, by default), NoiseLocation where it is (this actor's
	 * location by default: FVector::ZeroVector). Nothing hears it until a module sets the noise delegate.
	 */
	void MakeNoise(
		float Loudness = 1.0f, APawn* NoiseInstigator = nullptr, FVector NoiseLocation = FVector::ZeroVector);
	/** Who hears noises (UE: SetMakeNoiseDelegate); an empty function silences them. */
	static void SetMakeNoiseDelegate(const FMakeNoiseDelegate& NewDelegate);

	/** Destroys the actor after InLifespan seconds; 0 cancels (UE: SetLifeSpan). */
	virtual void SetLifeSpan(float InLifespan);
	/** Seconds left before the actor is destroyed, 0 without a life span (UE: GetLifeSpan). */
	[[nodiscard]] float GetLifeSpan() const;
	/** The life span ran out: destroys the actor (UE: LifeSpanExpired). */
	virtual void LifeSpanExpired();

	/** The actor's input bindings (UE: InputComponent): a player controller's or a possessed pawn's. */
	UPROPERTY(Transient)
	UInputComponent* InputComponent = nullptr;

	// UObject
	void BeginDestroy() override;

protected:
	/** Registers or unregisters the actor's own tick function (UE: RegisterActorTickFunctions). */
	virtual void RegisterActorTickFunctions(bool bRegister);

	/** The point damage an actor takes from a point event: the amount as it is (UE: InternalTakePointDamage). */
	virtual float InternalTakePointDamage(
		float Damage, const FPointDamageEvent& PointDamageEvent, AController* EventInstigator, AActor* DamageCauser);
	/**
	 * The radial damage an actor takes: the event's falloff at the closest component hit, between MinimumDamage and
	 * Damage (UE: InternalTakeRadialDamage).
	 */
	virtual float InternalTakeRadialDamage(
		float Damage, const FRadialDamageEvent& RadialDamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/**
	 * The root component's relative location / rotation, which are the actor's while the root is not attached (Leon:
	 * the kinematic character movement edits them in place).
	 */
	[[nodiscard]] FVector& MutableLocation()
	{
		return RootComponent->RelativeLocation;
	}
	[[nodiscard]] FRotator& MutableRotation()
	{
		return RootComponent->RelativeRotation;
	}

	/** The component whose transform is the actor's (UE: RootComponent). */
	UPROPERTY()
	USceneComponent* RootComponent = nullptr;

private:
	friend class UWorld;

	/**
	 * Every component whose outer is this actor, in creation order (UE: OwnedComponents, a TSet there). Not a
	 * UPROPERTY, as in UE: AddReferencedObjects reports it, and a template never copies it.
	 */
	TArray<UActorComponent*> OwnedComponents;

	UPROPERTY()
	AActor* Owner = nullptr;

	UPROPERTY()
	APawn* Instigator = nullptr;

	uint64 UniqueID = 0;
	/** UWorld gives it when the actor joins the level (GetLevelOrder). */
	uint64 LevelOrder = 0;
	/** The components' tick orders given so far (NextComponentTickSubOrder). */
	uint64 NumComponentTickOrders = 0;
	/** The life span's timer (UE: TimerHandle_LifeSpanExpired). */
	FTimerHandle TimerHandle_LifeSpanExpired;
	bool bActorIsBeingDestroyed = false;
	bool bActorInitialized = false;
	bool bActorHasBegunPlay = false;
	bool bActorBeginningPlay = false;
	bool bHasEndedPlay = false;
};
