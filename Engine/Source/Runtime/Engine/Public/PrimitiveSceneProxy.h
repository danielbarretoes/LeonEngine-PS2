#pragma once

#include "CoreMinimal.h"

class AActor;
class FReferenceCollector;
class FSceneView;
class UPrimitiveComponent;

/** Which proxy class a FPrimitiveSceneProxy is (Leon: no RTTI, so the renderer asks instead of casting blind). */
enum class EPrimitiveSceneProxyType : uint8
{
	StaticMesh,
	SkeletalMesh,
};

/**
 * What the renderer knows of a primitive component (UE: FPrimitiveSceneProxy): a snapshot taken by
 * UPrimitiveComponent::CreateSceneProxy when the component's render state is created, plus its world transform, which
 * the world sends at the end of each step (FSceneInterface::UpdatePrimitiveTransform). The scene owns the proxy. A
 * change the snapshot does not follow (a new mesh or material, the visibility) recreates it
 * (UActorComponent::MarkRenderStateDirty).
 *
 * Interpolation (ps2-shipping D4): the proxy keeps the transforms of the last two steps, and the render draws it
 * between them (InterpolateTransform, with the fixed step clock's remainder as the weight), so what moves moves
 * smoothly at any frame rate while the world steps at 30 Hz. The render's transform is the proxy's alone: nothing of it
 * goes back to the component. A new proxy, and one that jumped more than TeleportDistance in a step (a respawn), start
 * from the step's transform.
 *
 * The snapshot points at assets (a mesh, the textures of its materials): the scene reports them to the garbage
 * collector through AddReferencedObjects, so an asset outlives every proxy that draws it, even when its component let
 * go of it without recreating the proxy (Leon; UE fences the render thread instead).
 */
class ENGINE_API FPrimitiveSceneProxy
{
public:
	FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, EPrimitiveSceneProxyType InProxyType);
	virtual ~FPrimitiveSceneProxy() = default;

	FPrimitiveSceneProxy(const FPrimitiveSceneProxy&) = delete;
	FPrimitiveSceneProxy& operator=(const FPrimitiveSceneProxy&) = delete;

	[[nodiscard]] EPrimitiveSceneProxyType GetProxyType() const
	{
		return ProxyType;
	}

	/** A move longer than this in one step is a jump, drawn at once (cm). */
	static constexpr float TeleportDistance = 300.0f;

	/**
	 * Local to world, with the scale, as the render draws it (UE: GetLocalToWorld): the last step's transform, or
	 * between the last two steps after InterpolateTransform.
	 */
	[[nodiscard]] const FMatrix& GetLocalToWorld() const
	{
		return LocalToWorld;
	}
	/**
	 * The component's transform at the end of world step Step (UE: SetTransform; Leon passes the step, the bounds come
	 * from the mesh): a new step keeps the last one as the previous; the same step again only replaces it. The render
	 * draws the new transform until InterpolateTransform.
	 */
	void SetStepTransform(const FTransform& InTransform, uint32 Step);
	/** Draws the proxy Alpha of the way from the previous step's transform to the last one's (0: previous, 1: last). */
	void InterpolateTransform(float Alpha);
	/** It moved in the last step: InterpolateTransform places it between the two. */
	[[nodiscard]] bool IsInterpolated() const
	{
		return bInterpolate;
	}

	/** Drawn in game: the component is visible and its owner is not hidden (UE: IsShown). */
	[[nodiscard]] bool IsShown() const
	{
		return bShown;
	}
	/**
	 * Drawn in View (UE: IsShown(View)): shown, and for bOnlyOwnerSee the view's actor owns it, for bOwnerNoSee it
	 * does not (the owners are the component's owner and that actor's owners).
	 */
	[[nodiscard]] bool IsShown(const FSceneView* View) const;
	/** Drawn in the view model pass (UPrimitiveComponent::bRenderAsViewModel). */
	[[nodiscard]] bool IsViewModel() const
	{
		return bRenderAsViewModel;
	}
	/**
	 * The component's light is baked (UE: HasStaticLighting): a Static component, which the renderer never lights per
	 * frame; a Movable one is lit by the scene's lights every frame.
	 */
	[[nodiscard]] bool HasStaticLighting() const
	{
		return bStaticLighting;
	}
	/** The component casts shadows (UE: CastsDynamicShadow): its CastShadow, never for a view model. */
	[[nodiscard]] bool CastsDynamicShadow() const
	{
		return bCastDynamicShadow;
	}

	/** The primitive's bounds in the world, where the render draws it (a skeletal mesh's current pose's). */
	[[nodiscard]] virtual FBox GetWorldBounds() const = 0;

	/** The component casts a blob shadow (UPrimitiveComponent::bCastBlobShadow; never a view model's). */
	[[nodiscard]] bool CastsBlobShadow() const
	{
		return bCastBlobShadow;
	}
	/** The floor under the primitive (its component traces it, UpdateBlobShadowFloor), or none. */
	void SetBlobShadowFloor(bool bInHasFloor, const FVector& InPoint, const FVector& InNormal)
	{
		bHasBlobShadowFloor = bInHasFloor;
		BlobShadowFloorPoint = InPoint;
		BlobShadowFloorNormal = InNormal;
	}
	/** The floor's point and normal; false when there is no floor under it (or no blob shadow). */
	[[nodiscard]] bool GetBlobShadowFloor(FVector& OutPoint, FVector& OutNormal) const
	{
		OutPoint = BlobShadowFloorPoint;
		OutNormal = BlobShadowFloorNormal;
		return bCastBlobShadow && bHasBlobShadowFloor;
	}

	/** Reports the assets the snapshot points at (Leon: the scene calls it from its FGCObject). */
	virtual void AddReferencedObjects(FReferenceCollector& Collector)
	{
		(void)Collector;
	}

private:
	FMatrix LocalToWorld = FMatrix::Identity;
	/** The transforms of the last two steps, and the last one's step. */
	FTransform PreviousTransform = FTransform::Identity;
	FTransform CurrentTransform = FTransform::Identity;
	uint32 CurrentStep = 0;
	/** The two steps differ (and not by a jump). */
	bool bInterpolate = false;
	/** The actors that own the component, directly or through their owners, when an owner flag is set (UE: Owners). */
	TArray<const AActor*> Owners;
	EPrimitiveSceneProxyType ProxyType;
	bool bShown = true;
	bool bCastDynamicShadow = true;
	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;
	bool bRenderAsViewModel = false;
	bool bStaticLighting = false;
	bool bCastBlobShadow = false;
	bool bHasBlobShadowFloor = false;
	FVector BlobShadowFloorPoint = FVector::ZeroVector;
	FVector BlobShadowFloorNormal = FVector::UpVector;
};
