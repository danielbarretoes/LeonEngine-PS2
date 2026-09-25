#pragma once

#include "CoreMinimal.h"
#include "LightSceneProxy.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "UObject/GCObject.h"

class UActorComponent;

/** A primitive of the scene (UE: FPrimitiveSceneInfo): the component and the proxy the scene owns. */
struct FPrimitiveSceneInfo
{
	UPrimitiveComponent* Component = nullptr;
	TUniquePtr<FPrimitiveSceneProxy> Proxy;
	/** Where the primitive sits in the scene (FScene::GetOrderKey). */
	uint64 OrderKey = 0;
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
	void AddLight(ULightComponent* Light) override;
	void RemoveLight(ULightComponent* Light) override;
	void UpdateLightTransform(ULightComponent* Light) override;
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

	/** The component's place: its owner's spawn serial (AActor::GetUniqueID), then its index among the owner's. */
	[[nodiscard]] static uint64 GetOrderKey(const UActorComponent* Component);

	// FGCObject
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override
	{
		return TEXT("FScene");
	}

private:
	UWorld* World = nullptr;
	TArray<FPrimitiveSceneInfo> Primitives;
	TArray<FLightSceneInfo> Lights;
};
