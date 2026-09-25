#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshRenderData.h"
#include "StaticMeshRenderData.h"
#include "Texture2DResource.h"

class UObject;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/**
 * The renderer's GPU resources for the engine's asset UObjects, keyed by asset (Leon; UE keeps them on the assets:
 * UStaticMesh's RenderData, UTexture's Resource). A resource is made the first time its asset is drawn and lives until
 * its asset releases it (the asset's data changed, or BeginDestroy: IRendererModule::ReleaseAssetResources) or the
 * renderer shuts down, so every GL object is released while the context exists and an address is never reused for
 * another asset while it is cached. No render thread: everything happens on the game thread.
 */
class FRenderResourceCache
{
public:
	[[nodiscard]] const FStaticMeshRenderData& GetStaticMesh(const UStaticMesh& Mesh);
	[[nodiscard]] const FSkeletalMeshRenderData& GetSkeletalMesh(const USkeletalMesh& Mesh);
	[[nodiscard]] const FTexture2DResource& GetTexture(const UTexture2D& Texture);

	/** Frees every GPU resource. */
	void ReleaseResources();

	/** Frees the resource of one asset, if there is one. */
	void ReleaseResources(const UObject* Asset);

private:
	TMap<const UObject*, TUniquePtr<FStaticMeshRenderData>> StaticMeshes;
	TMap<const UObject*, TUniquePtr<FSkeletalMeshRenderData>> SkeletalMeshes;
	TMap<const UObject*, TUniquePtr<FTexture2DResource>> Textures;
};
