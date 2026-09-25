#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Material.h"
#include "MeshData.h"

/**
 * Path- and key-keyed cache of the legacy content (Leon): cooked `.lmesh` meshes, PNG textures, `.lmat` materials, the
 * procedural checker / bump maps and the basic cube / plane / sphere meshes, as CPU assets (UStaticMesh, UTexture2D,
 * FMaterial). The engine owns one (UGameEngine::GetResources) and the level reader fills it; the renderer keeps its own
 * GPU copies. P14 replaces it with asset loading (LoadObject / TSoftObjectPtr).
 */
class ENGINE_API FResourceCache
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

	/**
	 * When false, textures are not loaded (null maps) and the default material is a plain grey instead of the checker:
	 * the headless engine (`-nullrhi`) never draws them.
	 */
	void SetTextureLoadingEnabled(bool bEnabled)
	{
		bTextureLoadingEnabled = bEnabled;
	}
	[[nodiscard]] bool IsTextureLoadingEnabled() const
	{
		return bTextureLoadingEnabled;
	}

	void Clear();
	/** Drops a cached material so the next LoadMaterial reloads it from disk. */
	void InvalidateMaterial(const FString& Path);

private:
	[[nodiscard]] static FString NormalizeKey(const FString& Path);
	[[nodiscard]] TSharedPtr<UStaticMesh> CacheMesh(const FString& Key, const FMeshData& Data);
	[[nodiscard]] TSharedPtr<UTexture2D> CacheTexture(const FString& Key, UTexture2D&& Texture);

	bool bTextureLoadingEnabled = true;
	TMap<FString, TSharedPtr<UStaticMesh>> Meshes;
	TMap<FString, TSharedPtr<UTexture2D>> Textures;
	TMap<FString, FMaterial> Materials;
};
