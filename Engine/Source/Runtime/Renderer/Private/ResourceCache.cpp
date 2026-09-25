#include "ResourceCache.h"

#include "LeonMeshFormat.h"
#include "MaterialAsset.h"
#include "MeshData.h"
#include "Migration/LegacyContentPath.h"
#include "Primitives.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

std::string FResourceCache::NormalizeKey(const std::string& Path)
{
	std::error_code Ec;
	const std::string Key =
		std::filesystem::weakly_canonical(std::filesystem::path(Path), Ec).lexically_normal().string();
	return Ec ? Path : Key;
}

std::shared_ptr<UStaticMesh> FResourceCache::CacheMesh(const std::string& Key, FMeshData Data)
{
	if (const auto It = Meshes.find(Key); It != Meshes.end())
	{
		return It->second;
	}
	if (Data.IsEmpty())
	{
		return nullptr;
	}
	auto Mesh =
		std::make_shared<UStaticMesh>(bGpuUploadEnabled ? UStaticMesh::Upload(Data) : UStaticMesh::CreateCpu(Data));
	if (!Mesh->Valid())
	{
		return nullptr;
	}
	Meshes.emplace(Key, Mesh);
	return Mesh;
}

std::shared_ptr<UStaticMesh> FResourceCache::LoadStaticMesh(const std::string& Path)
{
	const std::string CacheKey = NormalizeKey(Path);
	if (const auto It = Meshes.find(CacheKey); It != Meshes.end())
	{
		return It->second;
	}

	FMeshData Data;
	if (IsLeonMeshPath(FString(Path.c_str())))
	{
		if (!LoadLeonMeshFile(FString(Path.c_str()), Data))
		{
			std::cerr << "ResourceCache: failed to load .lmesh '" << Path << "'\n";
			return nullptr;
		}
	}
	else
	{
		// Shipping / runtime: cooked `.lmesh` only (OBJ/FBX/glTF via Editor Import / leon-cook).
		std::cerr << "ResourceCache: expected .lmesh path, got '" << Path << "'\n";
		return nullptr;
	}

	// Bind diffuse textures referenced by the MTL before GPU upload.
	for (int32 I = 0; I < Data.Materials.Num() && I < Data.AlbedoMapPaths.Num(); ++I)
	{
		if (Data.AlbedoMapPaths[I].IsEmpty())
		{
			continue;
		}
		Data.Materials[I].AlbedoMap = LoadTexture(std::string(*Data.AlbedoMapPaths[I]));
		if (Data.Materials[I].AlbedoMap == nullptr)
		{
			std::cerr << "ResourceCache: missing albedo map '" << *Data.AlbedoMapPaths[I] << "'\n";
		}
	}

	return CacheMesh(CacheKey, std::move(Data));
}

TSharedPtr<UTexture2D> FResourceCache::LoadTexture(const std::string& Path)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	const std::string CacheKey = NormalizeKey(Path);
	if (const auto It = Textures.find(CacheKey); It != Textures.end())
	{
		return It->second;
	}

	auto Texture = MakeShared<UTexture2D>(UTexture2D::LoadFromFile(Path));
	if (!Texture->Valid())
	{
		return nullptr;
	}
	Textures.emplace(CacheKey, Texture);
	return Texture;
}

TSharedPtr<UTexture2D> FResourceCache::CheckerTexture(int Size)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	Size = std::max(Size, 2);
	const std::string CacheKey = "proc:checker:" + std::to_string(Size);
	if (const auto It = Textures.find(CacheKey); It != Textures.end())
	{
		return It->second;
	}

	auto Texture = MakeShared<UTexture2D>(UTexture2D::CreateChecker(Size));
	if (!Texture->Valid())
	{
		return nullptr;
	}
	Textures.emplace(CacheKey, Texture);
	return Texture;
}

TSharedPtr<UTexture2D> FResourceCache::BumpNormalTexture(int Size)
{
	if (!bGpuUploadEnabled)
	{
		return nullptr;
	}
	Size = std::max(Size, 8);
	const std::string CacheKey = "proc:normal:bump:" + std::to_string(Size);
	if (const auto It = Textures.find(CacheKey); It != Textures.end())
	{
		return It->second;
	}
	auto Texture = MakeShared<UTexture2D>(UTexture2D::CreateBumpNormal(Size));
	if (!Texture->Valid())
	{
		return nullptr;
	}
	Textures.emplace(CacheKey, Texture);
	return Texture;
}

FMaterial FResourceCache::LoadMaterial(const std::string& Path)
{
	const std::string CacheKey = NormalizeKey(Path);
	if (const auto It = Materials.find(CacheKey); It != Materials.end())
	{
		return It->second;
	}

	FMaterial Material;
	if (!LoadMaterialFile(*this, Path, Material))
	{
		std::cerr << "ResourceCache: using default material (failed '" << Path << "')\n";
		return DefaultMaterial();
	}
	Materials.emplace(CacheKey, Material);
	return Material;
}

FMaterial FResourceCache::DefaultMaterial()
{
	constexpr const char* Key = "engine:default";
	if (const auto It = Materials.find(Key); It != Materials.end())
	{
		return It->second;
	}

	FMaterial Material;
	const std::string LmatPath = ResolveLegacyContentPath("assets/Materials/M_Default.lmat");
	if (std::filesystem::exists(LmatPath) && LoadMaterialFile(*this, LmatPath, Material))
	{
		Materials.emplace(Key, Material);
		return Material;
	}

	if (bGpuUploadEnabled)
	{
		Material = MakeDefaultCheckerMaterial(*this);
	}
	else
	{
		Material.Shading = EMaterialShadingModel::BlinnPhong;
		Material.Albedo = {0.55f, 0.55f, 0.58f};
		Material.Shininess = 16.0f;
		Material.SyncRoughnessFromShininess();
	}
	Materials.emplace(Key, Material);
	return Material;
}

std::shared_ptr<UStaticMesh> FResourceCache::GetCubeMesh()
{
	return CacheMesh("proc:cube", MakeCube());
}

std::shared_ptr<UStaticMesh> FResourceCache::GetPlaneMesh(float Size, float UvScale)
{
	const std::string Key = "proc:plane:" + std::to_string(Size) + ":" + std::to_string(UvScale);
	return CacheMesh(Key, MakePlane(Size, UvScale));
}

std::shared_ptr<UStaticMesh> FResourceCache::GetSphereMesh(int Segments, int Rings)
{
	const std::string Key = "proc:sphere:" + std::to_string(Segments) + ":" + std::to_string(Rings);
	return CacheMesh(Key, MakeSphere(Segments, Rings));
}

void FResourceCache::Clear()
{
	Meshes.clear();
	Textures.clear();
	Materials.clear();
}

void FResourceCache::InvalidateMaterial(const std::string& Path)
{
	Materials.erase(NormalizeKey(Path));
}
