#pragma once

#include "CoreMinimal.h"
#include "LightSceneProxy.h"
#include "Misc/Scratchpad.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "UObject/GCObject.h"
#include "VisibilityCells.h"

/**
 * A list of the frame's render (UE: TArray<T, SceneRenderingAllocator>, on the frame's stack there): on the scratchpad
 * (N15), inside the scene renderer's FScratchpadMark.
 */
template <typename ElementType>
using FSceneRenderList = TArray<ElementType, TScratchpadAllocator<>>;

class FSceneView;
class FStaticMeshSceneProxy;
class UActorComponent;

/** A primitive of the scene (UE: FPrimitiveSceneInfo): the component and the proxy the scene owns. */
struct FPrimitiveSceneInfo
{
	UPrimitiveComponent* Component = nullptr;
	TUniquePtr<FPrimitiveSceneProxy> Proxy;
	/** Where the primitive sits in the scene (FScene::GetOrderKey). */
	uint64 OrderKey = 0;
	/**
	 * The cells of the scene's FVisibilityCellGraph its bounds touch, a bit each (0: none, or no cells), assigned when
	 * the cells or the primitive are added; a primitive that moves (not Static) is assigned again each frame.
	 */
	uint64 CellMask = 0;
};

/** A light of the scene (UE: FLightSceneInfo). */
struct FLightSceneInfo
{
	ULightComponent* Component = nullptr;
	TUniquePtr<FLightSceneProxy> Proxy;
	uint64 OrderKey = 0;
};

/**
 * The renderer's scene (UE: FScene, ScenePrivate.h): the proxies of a world's primitives and lights, which
 * FSceneRenderer draws. Primitives and lights are kept in the order of their actors' spawn, then of the component in
 * its actor (GetOrderKey), so a proxy recreated for a changed component keeps its place and the draw order does not
 * depend on when a component last changed.
 *
 * It is an FGCObject: the assets its proxies draw (meshes, textures) are reported to the garbage collector, even when
 * they are pending kill, so no asset is collected while a proxy points at it.
 */
class FScene final
	: public FSceneInterface
	, public FGCObject
{
public:
	explicit FScene(UWorld* InWorld);
	~FScene() override;

	// FSceneInterface
	void AddPrimitive(UPrimitiveComponent* Primitive) override;
	void RemovePrimitive(UPrimitiveComponent* Primitive) override;
	void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override;
	void InterpolateTransforms(float Alpha) override;
	void AddLight(ULightComponent* Light) override;
	void RemoveLight(ULightComponent* Light) override;
	void UpdateLightTransform(ULightComponent* Light) override;
	void UpdateVisibilityCells() override
	{
		bVisibilityCellsDirty = true;
	}
	[[nodiscard]] UWorld* GetWorld() const override
	{
		return World;
	}
	[[nodiscard]] FScene* GetRenderScene() override
	{
		return this;
	}
	[[nodiscard]] int32 GetNumPrimitives() const override
	{
		return Primitives.Num();
	}
	[[nodiscard]] int32 GetNumLights() const override
	{
		return Lights.Num();
	}

	[[nodiscard]] const TArray<FPrimitiveSceneInfo>& GetPrimitives() const
	{
		return Primitives;
	}
	[[nodiscard]] const TArray<FLightSceneInfo>& GetLights() const
	{
		return Lights;
	}

	/**
	 * The primitives View draws, in the scene's order (UE: FSceneRenderer's visibility): the world pass's static and
	 * skeletal meshes (shown, seen by the view's actor, and in a cell the view sees: VisibleCells) and the view model
	 * pass's (bRenderAsViewModel, shown in View: a first-person weapon and arms, seen by their owner only). The lists
	 * are the renderer's frame lists (on the scratchpad, N15). The skeletal meshes come as their infos, so the renderer
	 * can stamp the components it draws (UPrimitiveComponent::LastRenderTime). Returns how many the cells left out.
	 */
	int32 GatherPrimitives(const FSceneView& View, const FVisibilityCellGraph::FVisibleCells& VisibleCells,
		FSceneRenderList<const FStaticMeshSceneProxy*>& OutWorldMeshes,
		FSceneRenderList<const FStaticMeshSceneProxy*>& OutViewModelMeshes,
		FSceneRenderList<const FPrimitiveSceneInfo*>& OutWorldSkeletalMeshes,
		FSceneRenderList<const FPrimitiveSceneInfo*>& OutViewModelSkeletalMeshes);

	/**
	 * The map's cells and portals (N15), gathered from the level's AVisibilityCellVolume and AVisibilityPortal actors
	 * again when they changed (UpdateVisibilityCells), each primitive assigned to the cells it touches then; empty for
	 * a map without cells.
	 */
	const FVisibilityCellGraph& GetVisibilityCells();

	/** The component's place: its owner's spawn serial (AActor::GetUniqueID), then its index among the owner's. */
	[[nodiscard]] static uint64 GetOrderKey(const UActorComponent* Component);

	// FGCObject
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override
	{
		return TEXT("FScene");
	}

private:
	/** The cells a primitive's bounds touch (0 without cells). */
	[[nodiscard]] uint64 GetCellMask(const FPrimitiveSceneProxy& Proxy) const;

	UWorld* World = nullptr;
	TArray<FPrimitiveSceneInfo> Primitives;
	TArray<FLightSceneInfo> Lights;
	FVisibilityCellGraph VisibilityCells;
	bool bVisibilityCellsDirty = true;
};
