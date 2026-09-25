#pragma once

#include "CoreMinimal.h"

class FReferenceCollector;
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
 * the world sends again before each frame (FSceneInterface::UpdatePrimitiveTransform). The scene owns the proxy. A
 * change the snapshot does not follow (a new mesh or material, the visibility) recreates it
 * (UActorComponent::MarkRenderStateDirty).
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

	/** Local to world, with the scale (UE: GetLocalToWorld). */
	[[nodiscard]] const FMatrix& GetLocalToWorld() const
	{
		return LocalToWorld;
	}
	/** UE: SetTransform (Leon passes only the matrix; the bounds come from the mesh). */
	void SetTransform(const FMatrix& InLocalToWorld)
	{
		LocalToWorld = InLocalToWorld;
	}

	/** Drawn in game: the component is visible and its owner is not hidden (UE: IsShown). */
	[[nodiscard]] bool IsShown() const
	{
		return bShown;
	}
	/** The component casts shadows (UE: CastsDynamicShadow; the materials decide per section). */
	[[nodiscard]] bool CastsDynamicShadow() const
	{
		return bCastDynamicShadow;
	}

	/** Reports the assets the snapshot points at (Leon: the scene calls it from its FGCObject). */
	virtual void AddReferencedObjects(FReferenceCollector& Collector)
	{
		(void)Collector;
	}

private:
	FMatrix LocalToWorld = FMatrix::Identity;
	EPrimitiveSceneProxyType ProxyType;
	bool bShown = true;
	bool bCastDynamicShadow = true;
};
