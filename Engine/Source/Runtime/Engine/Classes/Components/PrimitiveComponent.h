#pragma once

#include "CollisionResponseContainer.h"
#include "CollisionShape.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "PrimitiveComponent.generated.h"

class FPrimitiveSceneProxy;

/**
 * A scene component with geometry: something drawn, collided with or both (UE: UPrimitiveComponent).
 *
 * Its render state is its scene proxy: registering in a world adds it to the world's scene
 * (CreateRenderState_Concurrent → FSceneInterface::AddPrimitive → CreateSceneProxy), and the world sends the moved
 * transforms before each frame (UWorld::SendAllEndOfFrameUpdates). A world without a scene (`-nullrhi`) keeps none.
 *
 * Its physics state is its body in the world's FPhysScene: CreatePhysicsState adds one when the collision is enabled
 * (FPhysScene::AddComponentBody) and DestroyPhysicsState removes it. Changing the collision settings of a registered
 * component recreates the body. The body takes the component's object type and responses to the collision channels
 * (UE's collision settings; Leon has no named profiles, see UCollisionProfile).
 */
UCLASS(Abstract)
class ENGINE_API UPrimitiveComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPrimitiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Casts a shadow (UE: CastShadow). */
	UPROPERTY()
	uint8 CastShadow : 1;

	/**
	 * Drawn only in the views of its owner or its owner's owners (UE: bOnlyOwnerSee): a player's first-person weapon.
	 * The view's actor is the player's view target (FSceneView::ViewActor).
	 */
	UPROPERTY()
	uint8 bOnlyOwnerSee : 1;

	/** Drawn in every view but its owners' (UE: bOwnerNoSee): a player's own body. */
	UPROPERTY()
	uint8 bOwnerNoSee : 1;

	/**
	 * Drawn in the view model pass (Leon; UE 4.27 has none): after the scene, over a cleared depth buffer, with the
	 * camera's ViewModelFOV (FSceneView::ViewModelProjectionMatrix), so a first-person weapon never goes into a wall
	 * and keeps its own field of view. It casts no shadow and is left out of the scene's other passes.
	 */
	UPROPERTY()
	uint8 bRenderAsViewModel : 1;

	/** UE: SetOnlyOwnerSee / SetOwnerNoSee; Leon: SetRenderAsViewModel. Each recreates the render state. */
	void SetOnlyOwnerSee(bool bNewOnlyOwnerSee);
	void SetOwnerNoSee(bool bNewOwnerNoSee);
	void SetRenderAsViewModel(bool bNewRenderAsViewModel);

	/** Reports overlaps (UE: bGenerateOverlapEvents; kept for the UE shape, nothing queries overlaps yet). */
	UPROPERTY()
	uint8 bGenerateOverlapEvents : 1;

	/**
	 * Actors this component's sweeps ignore when it moves (UE: MoveIgnoreActors): a projectile ignores the pawn that
	 * fired it (UProjectileMovementComponent).
	 */
	UPROPERTY(Transient)
	TArray<AActor*> MoveIgnoreActors;

	/** Adds or removes an actor MoveIgnoreActors holds (UE: IgnoreActorWhenMoving). */
	void IgnoreActorWhenMoving(AActor* Actor, bool bShouldIgnore);

	/**
	 * What the collision takes part in (UE: BodyInstance.CollisionEnabled). Leon's default is NoCollision: the physics
	 * scene holds the level geometry, which a map enables per actor, and the characters' capsules (query only, P17).
	 */
	void SetCollisionEnabled(ECollisionEnabled NewType);
	[[nodiscard]] ECollisionEnabled GetCollisionEnabled() const
	{
		return CollisionEnabled;
	}
	/** True unless the collision is NoCollision (UE: IsCollisionEnabled). */
	[[nodiscard]] bool IsCollisionEnabled() const
	{
		return CollisionEnabled != ECollisionEnabled::NoCollision;
	}
	/** A simulated (Dynamic) body instead of a static one (UE: SetSimulatePhysics / IsSimulatingPhysics). */
	void SetSimulatePhysics(bool bSimulate);
	[[nodiscard]] bool IsSimulatingPhysics() const
	{
		return bSimulatePhysics;
	}
	/** Gravity on the simulated body (UE: SetEnableGravity / IsGravityEnabled). */
	void SetEnableGravity(bool bGravityEnabled);
	[[nodiscard]] bool IsGravityEnabled() const
	{
		return bEnableGravity;
	}

	/** What the body is to queries on the collision channels (UE: SetCollisionObjectType / GetCollisionObjectType). */
	void SetCollisionObjectType(ECollisionChannel Channel);
	[[nodiscard]] ECollisionChannel GetCollisionObjectType() const
	{
		return ObjectType.GetValue();
	}
	/** How the body answers one channel (UE: SetCollisionResponseToChannel / GetCollisionResponseToChannel). */
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
	[[nodiscard]] ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const
	{
		return CollisionResponses.GetResponse(Channel);
	}
	/** Every channel at once (UE: SetCollisionResponseToAllChannels). */
	void SetCollisionResponseToAllChannels(ECollisionResponse NewResponse);
	/** All the responses (UE: SetCollisionResponseToChannels / GetCollisionResponseToChannels). */
	void SetCollisionResponseToChannels(const FCollisionResponseContainer& NewResponses);
	[[nodiscard]] const FCollisionResponseContainer& GetCollisionResponseToChannels() const
	{
		return CollisionResponses;
	}

	/**
	 * Sends the component's transform to its body (UE: SendPhysicsTransform, which moving a component calls). Leon's
	 * components do not track their moves: the code that moves a colliding component calls it (the character does
	 * after its movement).
	 */
	void SendPhysicsTransform();

	/**
	 * Whether the component's body is part of the navigation data (UE: CanEverAffectNavigation). The waypoint
	 * navigation (P20) builds no data from the bodies, so nothing reads it now; UE's components keep setting it.
	 */
	[[nodiscard]] bool CanEverAffectNavigation() const
	{
		return bCanEverAffectNavigation;
	}
	void SetCanEverAffectNavigation(bool bRelevant)
	{
		bCanEverAffectNavigation = bRelevant;
	}

	/** The collision shape in world units, grown by Inflation (UE: GetCollisionShape); a line by default. */
	[[nodiscard]] virtual FCollisionShape GetCollisionShape(float Inflation = 0.0f) const;

	/**
	 * The renderer's snapshot of the component (UE: CreateSceneProxy), owned by the scene; null when there is nothing
	 * to draw (no mesh, a collision shape).
	 */
	[[nodiscard]] virtual FPrimitiveSceneProxy* CreateSceneProxy();

	/** True when the world should draw it: visible, and its owner is not hidden (UE: ShouldRender). */
	[[nodiscard]] bool ShouldRender() const;

	/** Sends the world transform to the proxy (FSceneInterface::UpdatePrimitiveTransform). */
	void SendRenderTransform_Concurrent() override;

	/** The proxy the scene made from the component, while it is in a scene (UE: SceneProxy). */
	FPrimitiveSceneProxy* SceneProxy = nullptr;

protected:
	void CreateRenderState_Concurrent() override;
	void DestroyRenderState_Concurrent() override;
	void CreatePhysicsState() override;
	void DestroyPhysicsState() override;

private:
	// UE keeps these in the component's FBodyInstance (BodyInstance, a USTRUCT); Leon's physics scene owns its bodies,
	// so they are the component's own properties (saved with a map).

	/** UE: BodyInstance.CollisionEnabled. */
	UPROPERTY()
	ECollisionEnabled CollisionEnabled = ECollisionEnabled::NoCollision;

	/** UE: BodyInstance.bSimulatePhysics. */
	UPROPERTY()
	bool bSimulatePhysics = false;

	/** UE: BodyInstance.bEnableGravity. */
	UPROPERTY()
	bool bEnableGravity = true;

	/** UE: BodyInstance.ObjectType (WorldStatic by default, as UE's). */
	UPROPERTY()
	TEnumAsByte<ECollisionChannel> ObjectType = ECC_WorldStatic;

	/** UE: BodyInstance.CollisionResponses (every channel blocks by default). */
	UPROPERTY()
	FCollisionResponseContainer CollisionResponses;

	/** UE: UActorComponent::bCanEverAffectNavigation (true by default; a character's capsule is left out). */
	UPROPERTY()
	bool bCanEverAffectNavigation = true;
};
