#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "ActorComponent.generated.h"

class AActor;
class UWorld;

/**
 * Reusable behavior an actor owns (UE: UActorComponent): no transform, registration with the owner's world, tick and
 * play hooks.
 *
 * Lifecycle (UE):
 * - Created with CreateDefaultSubobject in the owner's constructor, or with NewObject<T>(Actor) later. Either way the
 *   outer is the actor, and PostInitProperties adds the component to the actor's OwnedComponents (which keeps it
 *   alive).
 * - Registered when its actor is spawned (AActor::RegisterAllComponents, bAutoRegister), or with RegisterComponent for
 *   one created after the spawn. Registration runs OnRegister, then CreateRenderState_Concurrent and
 *   CreatePhysicsState when the actor is in a world, then BeginPlay when the actor has already begun play.
 * - InitializeComponent (with bWantsInitializeComponent) runs once, from AActor::InitializeComponents at spawn.
 * - DestroyComponent ends play, unregisters, removes it from its owner and marks it pending kill; the next garbage
 *   collection frees it.
 */
UCLASS(Abstract)
class ENGINE_API UActorComponent : public UObject
{
	GENERATED_BODY()

public:
	UActorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Tags for gameplay code (UE: ComponentTags). */
	UPROPERTY()
	TArray<FName> ComponentTags;

	/** Registered when the owning actor is spawned (UE: bAutoRegister). */
	UPROPERTY()
	uint8 bAutoRegister : 1;

	/** InitializeComponent is called at spawn (UE: bWantsInitializeComponent). */
	UPROPERTY()
	uint8 bWantsInitializeComponent : 1;

	/** The owning actor (UE: GetOwner): the actor outer, cached when the component is created. */
	[[nodiscard]] AActor* GetOwner() const
	{
		return OwnerPrivate;
	}

	/** The owning actor's world while registered (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const;

	[[nodiscard]] bool IsRegistered() const
	{
		return bRegistered;
	}
	[[nodiscard]] bool HasBeenInitialized() const
	{
		return bHasBeenInitialized;
	}
	[[nodiscard]] bool HasBegunPlay() const
	{
		return bHasBegunPlay;
	}
	[[nodiscard]] bool IsBeingDestroyed() const
	{
		return bIsBeingDestroyed;
	}

	/** Whether TickComponent runs each world tick (UE: IsComponentTickEnabled). Off by default. */
	[[nodiscard]] bool IsComponentTickEnabled() const
	{
		return bComponentTickEnabled;
	}
	void SetComponentTickEnabled(bool bEnabled)
	{
		bComponentTickEnabled = bEnabled;
	}

	/** True when the component has a tag (UE: ComponentHasTag). */
	[[nodiscard]] bool ComponentHasTag(FName Tag) const
	{
		return ComponentTags.Contains(Tag);
	}

	/** Registers with the owner's world (UE: RegisterComponent); for components created after the actor spawned. */
	void RegisterComponent();
	/** Registers with InWorld, which may be null for an actor outside any world (UE: RegisterComponentWithWorld). */
	void RegisterComponentWithWorld(UWorld* InWorld);
	/** Destroys the render and physics state and runs OnUnregister (UE: UnregisterComponent). */
	void UnregisterComponent();

	/** Destroys and creates the physics state again, for a registered component in a world (UE: RecreatePhysicsState).
	 */
	void RecreatePhysicsState();

	/**
	 * Ends play, unregisters, removes the component from its owner and marks it pending kill (UE: DestroyComponent).
	 */
	virtual void DestroyComponent(bool bPromoteChildren = false);

	/** Called once from AActor::InitializeComponents when bWantsInitializeComponent (UE). */
	virtual void InitializeComponent();
	virtual void UninitializeComponent();

	/** Overrides call Super::BeginPlay() (UE). */
	virtual void BeginPlay();
	/** Overrides call Super::EndPlay(EndPlayReason) (UE). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/** Only called while IsComponentTickEnabled() (UE: TickComponent without the tick function arguments). */
	virtual void TickComponent(float DeltaTime);

	// UObject
	void PostInitProperties() override;
	void BeginDestroy() override;

protected:
	/** Registration hooks (UE): OnRegister first, OnUnregister last. Overrides call Super. */
	virtual void OnRegister();
	virtual void OnUnregister();

	/**
	 * The render state: what the renderer needs to draw the component (UE: CreateRenderState_Concurrent; P13 moves
	 * this to FScene::AddPrimitive). Only called when the component registers in a world.
	 */
	virtual void CreateRenderState_Concurrent();
	virtual void DestroyRenderState_Concurrent();
	/**
	 * The physics state (UE: CreatePhysicsState): UPrimitiveComponent adds its body to the world's physics scene.
	 * Only called when the component registers in a world.
	 */
	virtual void CreatePhysicsState();
	virtual void DestroyPhysicsState();

	[[nodiscard]] bool IsRenderStateCreated() const
	{
		return bRenderStateCreated;
	}
	[[nodiscard]] bool IsPhysicsStateCreated() const
	{
		return bPhysicsStateCreated;
	}

private:
	friend class AActor;

	/** The owning actor (UE: OwnerPrivate), an UPROPERTY so a pending-kill owner is cleared by the collector. */
	UPROPERTY(Transient)
	AActor* OwnerPrivate = nullptr;

	/** The world the component registered with (UE: WorldPrivate). */
	UPROPERTY(Transient)
	UWorld* WorldPrivate = nullptr;

	bool bRegistered = false;
	bool bRenderStateCreated = false;
	bool bPhysicsStateCreated = false;
	bool bHasBeenInitialized = false;
	bool bHasBegunPlay = false;
	bool bIsBeingDestroyed = false;
	bool bComponentTickEnabled = false;
};
