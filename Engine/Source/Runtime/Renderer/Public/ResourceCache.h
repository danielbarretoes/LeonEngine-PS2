#pragma once

#include "CoreMinimal.h"
#include "Material.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include "Texture2D.h"

/**
 * Path- and key-keyed cache for GPU meshes/textures: cooked mesh loads plus
 * procedural checker/bump normals, material assets, and cube/plane/sphere meshes.
 */
class RENDERER_API FResourceCache
{
public:
	[[nodiscard]] TSharedPtr<UStaticMesh> LoadStaticMesh(const FString& Path);
	[[nodiscard]] TSharedPtr<UTexture2D> LoadTexture(const FString& Path);
	[[nodiscard]] TSharedPtr<UTexture2D> CheckerTexture(int32 Size = 64);
	[[nodiscard]] TSharedPtr<UTexture2D> BumpNormalTexture(int32 Size = 256);

	/** Loads a .lmat material asset (cached by resolved path). On failure returns DefaultMaterial(). */
	[[nodiscard]] FMaterial LoadMaterial(const FString& Path);
	/** Grayscale checker template (Unreal-like default / WorldGrid placeholder). */
	[[nodiscard]] FMaterial DefaultMaterial();

	[[nodiscard]] TSharedPtr<UStaticMesh> GetCubeMesh();
	[[nodiscard]] TSharedPtr<UStaticMesh> GetPlaneMesh(float Size = 800.0f, float UvScale = 4.0f);
	[[nodiscard]] TSharedPtr<UStaticMesh> GetSphereMesh(int32 Segments = 24, int32 Rings = 16);

	/** When false, meshes stay CPU-only and textures are skipped (headless). */
	void SetGpuUploadEnabled(bool bEnabled)
	{
		bGpuUploadEnabled = bEnabled;
	}
	[[nodiscard]] bool IsGpuUploadEnabled() const
	{
		return bGpuUploadEnabled;
	}

	void Clear();
	/** Drops a cached material so the next LoadMaterial reloads it from disk. */
	void InvalidateMaterial(const FString& Path);

private:
	[[nodiscard]] static FString NormalizeKey(const FString& Path);
	[[nodiscard]] TSharedPtr<UStaticMesh> CacheMesh(const FString& Key, const FMeshData& Data);
	[[nodiscard]] TSharedPtr<UTexture2D> CacheTexture(const FString& Key, UTexture2D&& Texture);

	bool bGpuUploadEnabled = true;
	TMap<FString, TSharedPtr<UStaticMesh>> Meshes;
	TMap<FString, TSharedPtr<UTexture2D>> Textures;
	TMap<FString, FMaterial> Materials;
};
