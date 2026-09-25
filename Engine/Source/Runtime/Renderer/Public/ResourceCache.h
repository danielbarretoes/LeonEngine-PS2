#pragma once

#include "Material.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include "Texture2D.h"

#include <memory>
#include <string>
#include <unordered_map>

/// Path- and key-keyed cache for GPU meshes/textures: OBJ/file loads plus
/// procedural checker/bump normals, material assets, and cube/plane/sphere meshes.
class RENDERER_API FResourceCache
{
public:
	[[nodiscard]] std::shared_ptr<UStaticMesh> LoadStaticMesh(const std::string& Path);
	[[nodiscard]] TSharedPtr<UTexture2D> LoadTexture(const std::string& Path);
	[[nodiscard]] TSharedPtr<UTexture2D> CheckerTexture(int Size = 64);
	[[nodiscard]] TSharedPtr<UTexture2D> BumpNormalTexture(int Size = 256);

	/// Load `.lmat` material asset (cached by resolved path). On failure → DefaultMaterial().
	[[nodiscard]] FMaterial LoadMaterial(const std::string& Path);
	/// Grayscale checker template (Unreal-like default / WorldGrid placeholder).
	[[nodiscard]] FMaterial DefaultMaterial();

	[[nodiscard]] std::shared_ptr<UStaticMesh> GetCubeMesh();
	[[nodiscard]] std::shared_ptr<UStaticMesh> GetPlaneMesh(float Size = 8.0f, float UvScale = 4.0f);
	[[nodiscard]] std::shared_ptr<UStaticMesh> GetSphereMesh(int Segments = 24, int Rings = 16);

	/// When false, meshes stay CPU-only and textures/env maps are skipped (headless server).
	void SetGpuUploadEnabled(bool bEnabled)
	{
		bGpuUploadEnabled = bEnabled;
	}
	[[nodiscard]] bool IsGpuUploadEnabled() const
	{
		return bGpuUploadEnabled;
	}

	void Clear();
	/// Drop a cached material so the next `loadMaterial` reloads from disk.
	void InvalidateMaterial(const std::string& Path);

private:
	[[nodiscard]] static std::string NormalizeKey(const std::string& Path);
	[[nodiscard]] std::shared_ptr<UStaticMesh> CacheMesh(const std::string& Key, FMeshData Data);

	bool bGpuUploadEnabled = true;
	std::unordered_map<std::string, std::shared_ptr<UStaticMesh>> Meshes;
	std::unordered_map<std::string, TSharedPtr<UTexture2D>> Textures;
	std::unordered_map<std::string, FMaterial> Materials;
};
