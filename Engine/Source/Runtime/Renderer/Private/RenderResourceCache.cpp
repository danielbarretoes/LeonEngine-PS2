#include "RenderResourceCache.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

namespace
{

	template <typename ResourceType, typename AssetType>
	const ResourceType& FindOrCreate(TMap<const UObject*, TUniquePtr<ResourceType>>& Map, const AssetType& Asset)
	{
		if (TUniquePtr<ResourceType>* Found = Map.Find(&Asset))
		{
			return **Found;
		}
		TUniquePtr<ResourceType>& Entry = Map.Add(&Asset);
		Entry = MakeUnique<ResourceType>(Asset);
		return *Entry;
	}

} // namespace

const FStaticMeshRenderData& FRenderResourceCache::GetStaticMesh(const UStaticMesh& Mesh)
{
	return FindOrCreate(StaticMeshes, Mesh);
}

const FSkeletalMeshRenderData& FRenderResourceCache::GetSkeletalMesh(const USkeletalMesh& Mesh)
{
	return FindOrCreate(SkeletalMeshes, Mesh);
}

const FTexture2DResource& FRenderResourceCache::GetTexture(const UTexture2D& Texture)
{
	return FindOrCreate(Textures, Texture);
}

void FRenderResourceCache::ReleaseResources(const UObject* Asset)
{
	StaticMeshes.Remove(Asset);
	SkeletalMeshes.Remove(Asset);
	Textures.Remove(Asset);
}

void FRenderResourceCache::ReleaseResources()
{
	StaticMeshes.Empty();
	SkeletalMeshes.Empty();
	Textures.Empty();
}
