#include "ResourceCache.h"

#include "LeonMeshFormat.h"
#include "MaterialAsset.h"
#include "MeshData.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "RendererLog.h"

FString FResourceCache::NormalizeKey(const FString& Path)
{
	FString Key = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Key);
	FPaths::CollapseRelativeDirectories(Key);
	return Key;
}

TSharedPtr<UStaticMesh> FResourceCache::CacheMesh(const FString& Key, const FMeshData& Data)
{
	if (const TSharedPtr<UStaticMesh>* Found = Meshes.Find(Key))
	{
		return *Found;
	}
	if (Data.IsEmpty())
	{
		return nullptr;
	}
	TSharedPtr<UStaticMesh> Mesh =
		MakeShared<UStaticMesh>(bGpuUploadEnabled ? UStaticMesh::Upload(Data) : UStaticMesh::CreateCpu(Data));
	if (!Mesh->Valid())
	{
		return nullptr;
	}
	Meshes.Add(Key, Mesh);
	return Mesh;
}

TSharedPtr<UTexture2D> FResourceCache::CacheTexture(const FString& Key, UTexture2D&& Texture)
{
	TSharedPtr<UTexture2D> Shared = MakeShared<UTexture2D>(MoveTemp(Texture));
	if (!Shared->Valid())
	{
		return nullptr;
	}
	Textures.Add(Key, Shared);
	return Shared;
}

TSharedPtr<UStaticMesh> FResourceCache::LoadStaticMesh(const FString& Path)
{
	const FString CacheKey = NormalizeKey(Path);
	if (const TSharedPtr<UStaticMesh>* Found = Meshes.Find(CacheKey))
	{
		return *Found;
	}

	// Shipping / runtime: cooked .lmesh only (OBJ/FBX/glTF go through LeonCook).
	if (!IsLeonMeshPath(Path))
	{
		UE_LOG(LogRenderer, Error, "ResourceCache: expected .lmesh path, got '%s'", *Path);
		return nullptr;
	}
	FMeshData Data;
	if (!LoadLeonMeshFile(Path, Data))
	{
		UE_LOG(LogRenderer, Error, "ResourceCache: failed to load .lmesh '%s'", *Path);
		return nullptr;
	}

	// Bind diffuse textures referenced by the MTL before GPU upload.
	for (int32 I = 0; I < Data.Materials.Num() && I < Data.AlbedoMapPaths.Num(); ++I)
	{
		if (Data.AlbedoMapPaths[I].IsEmpty())
		{
			continue;
		}
		Data.Materials[I].AlbedoMap = LoadTexture(Data.AlbedoMapPaths[I]);
		if (Data.Materials[I].AlbedoMap == nullptr)
		{
			UE_LOG(LogRenderer, Error, "ResourceCache: missing albedo map '%s'", *Data.AlbedoMapPaths[I]);
		}
	}

	return CacheMesh(CacheKey, Data);
}

TSharedPtr<UTexture2D> FResourceCache::LoadTexture(const FString& Path)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	const FString CacheKey = NormalizeKey(Path);
	if (const TSharedPtr<UTexture2D>* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	return CacheTexture(CacheKey, UTexture2D::LoadFromFile(Path));
}

TSharedPtr<UTexture2D> FResourceCache::CheckerTexture(int32 Size)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	Size = FMath::Max(Size, 2);
	const FString CacheKey = FString::Printf("proc:checker:%d", Size);
	if (const TSharedPtr<UTexture2D>* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	return CacheTexture(CacheKey, UTexture2D::CreateChecker(Size));
}

TSharedPtr<UTexture2D> FResourceCache::BumpNormalTexture(int32 Size)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	Size = FMath::Max(Size, 8);
	const FString CacheKey = FString::Printf("proc:normal:bump:%d", Size);
	if (const TSharedPtr<UTexture2D>* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	return CacheTexture(CacheKey, UTexture2D::CreateBumpNormal(Size));
}

FMaterial FResourceCache::LoadMaterial(const FString& Path)
{
	const FString CacheKey = NormalizeKey(Path);
	if (const FMaterial* Found = Materials.Find(CacheKey))
	{
		return *Found;
	}

	FMaterial Material;
	if (!LoadMaterialFile(*this, Path, Material))
	{
		UE_LOG(LogRenderer, Warning, "ResourceCache: using default material (failed '%s')", *Path);
		return DefaultMaterial();
	}
	Materials.Add(CacheKey, Material);
	return Material;
}

FMaterial FResourceCache::DefaultMaterial()
{
	const FString Key("engine:default");
	if (const FMaterial* Found = Materials.Find(Key))
	{
		return *Found;
	}

	FMaterial Material;
	const FString LmatPath = FPaths::ResolveLegacyContentPath("assets/Materials/M_Default.lmat");
	if (FPaths::FileExists(LmatPath) && LoadMaterialFile(*this, LmatPath, Material))
	{
		Materials.Add(Key, Material);
		return Material;
	}

	if (bGpuUploadEnabled)
	{
		Material = MakeDefaultCheckerMaterial(*this);
	}
	else
	{
		Material.Shading = EMaterialShadingModel::BlinnPhong;
		Material.Albedo = FVector(0.55f, 0.55f, 0.58f);
		Material.Shininess = 16.0f;
		Material.SyncRoughnessFromShininess();
	}
	Materials.Add(Key, Material);
	return Material;
}

TSharedPtr<UStaticMesh> FResourceCache::GetCubeMesh()
{
	return CacheMesh("proc:cube", MakeCube());
}

TSharedPtr<UStaticMesh> FResourceCache::GetPlaneMesh(float Size, float UvScale)
{
	const FString Key = FString::Printf("proc:plane:%f:%f", static_cast<double>(Size), static_cast<double>(UvScale));
	return CacheMesh(Key, MakePlane(Size, UvScale));
}

TSharedPtr<UStaticMesh> FResourceCache::GetSphereMesh(int32 Segments, int32 Rings)
{
	const FString Key = FString::Printf("proc:sphere:%d:%d", Segments, Rings);
	return CacheMesh(Key, MakeSphere(Segments, Rings));
}

void FResourceCache::Clear()
{
	Meshes.Empty();
	Textures.Empty();
	Materials.Empty();
}

void FResourceCache::InvalidateMaterial(const FString& Path)
{
	Materials.Remove(NormalizeKey(Path));
}
