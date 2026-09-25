#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshRenderData.h"
#include "StaticMeshRenderData.h"
#include "Texture2DResource.h"

class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/**
 * The renderer's own GPU resources for the engine's CPU assets (Leon; UE keeps them on the assets: UStaticMesh's
 * RenderData, UTexture's Resource). A resource is made the first time its asset is drawn and lives until
 * ReleaseResources (the renderer's shutdown), so every GL object is released while the context exists. An entry keeps
 * its asset alive (the TSharedPtr), so an asset's address is never reused for another one while it is cached.
 */
class FRenderResourceCache
{
public:
	[[nodiscard]] const FStaticMeshRenderData& GetStaticMesh(const TSharedPtr<UStaticMesh>& Mesh);
	[[nodiscard]] const FSkeletalMeshRenderData& GetSkeletalMesh(const TSharedPtr<USkeletalMesh>& Mesh);
	[[nodiscard]] const FTexture2DResource& GetTexture(const TSharedPtr<UTexture2D>& Texture);

	/** Frees every GPU resource (and lets go of the assets). */
	void ReleaseResources();

private:
	template <typename AssetType, typename ResourceType>
	struct TCachedResource
	{
		TSharedPtr<AssetType> Asset;
		TUniquePtr<ResourceType> Resource;
	};

	TMap<const UStaticMesh*, TCachedResource<UStaticMesh, FStaticMeshRenderData>> StaticMeshes;
	TMap<const USkeletalMesh*, TCachedResource<USkeletalMesh, FSkeletalMeshRenderData>> SkeletalMeshes;
	TMap<const UTexture2D*, TCachedResource<UTexture2D, FTexture2DResource>> Textures;
};
