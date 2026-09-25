#pragma once

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
 * component recreates the body.
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

	/** Reports overlaps (UE: bGenerateOverlapEvents; kept for the UE shape, nothing queries overlaps yet). */
	UPROPERTY()
	uint8 bGenerateOverlapEvents : 1;

	/**
	 * What the collision takes part in (UE: BodyInstance.CollisionEnabled). Leon's default is NoCollision: the physics
	 * scene only holds level geometry, which the level reader enables per record, and characters are swept capsules,
	 * never bodies.
	 */
	void SetCollisionEnabled(ECollisionEnabled::Type NewType);
	[[nodiscard]] ECollisionEnabled::Type GetCollisionEnabled() const
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
	/** UE keeps these in the component's FBodyInstance (BodyInstance); Leon's physics scene owns its bodies. */
	ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
	bool bSimulatePhysics = false;
	bool bEnableGravity = true;
};
