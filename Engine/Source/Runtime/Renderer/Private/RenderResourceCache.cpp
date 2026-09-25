#include "RenderResourceCache.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

namespace
{

	template <typename MapType, typename AssetType, typename ResourceType>
	const ResourceType& FindOrCreate(MapType& Map, const TSharedPtr<AssetType>& Asset)
	{
		if (auto* Found = Map.Find(Asset.Get()))
		{
			return *Found->Resource;
		}
		auto& Entry = Map.Add(Asset.Get());
		Entry.Asset = Asset;
		Entry.Resource = MakeUnique<ResourceType>(*Asset);
		return *Entry.Resource;
	}

} // namespace

const FStaticMeshRenderData& FRenderResourceCache::GetStaticMesh(const TSharedPtr<UStaticMesh>& Mesh)
{
	return FindOrCreate<decltype(StaticMeshes), UStaticMesh, FStaticMeshRenderData>(StaticMeshes, Mesh);
}

const FSkeletalMeshRenderData& FRenderResourceCache::GetSkeletalMesh(const TSharedPtr<USkeletalMesh>& Mesh)
{
	return FindOrCreate<decltype(SkeletalMeshes), USkeletalMesh, FSkeletalMeshRenderData>(SkeletalMeshes, Mesh);
}

const FTexture2DResource& FRenderResourceCache::GetTexture(const TSharedPtr<UTexture2D>& Texture)
{
	return FindOrCreate<decltype(Textures), UTexture2D, FTexture2DResource>(Textures, Texture);
}

void FRenderResourceCache::ReleaseResources()
{
	StaticMeshes.Empty();
	SkeletalMeshes.Empty();
	Textures.Empty();
}
