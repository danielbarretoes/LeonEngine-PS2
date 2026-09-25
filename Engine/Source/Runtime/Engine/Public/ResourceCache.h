#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "MeshData.h"
#include "UObject/GCObject.h"

class UStaticMesh;
class UTexture2D;

/**
 * Path- and key-keyed cache of the legacy content (Leon): cooked `.lmesh` meshes, PNG textures, `.lmat` materials, the
 * procedural checker / bump maps and the basic cube / plane / sphere meshes. The meshes and textures are asset
 * UObjects (UStaticMesh, UTexture2D) in the transient package, which the cache keeps alive (FGCObject) until Clear;
 * the materials are FMaterial values. The engine owns one (UGameEngine::GetResources) and the level reader fills it;
 * the renderer keeps its own GPU copies. P14's legacy asset loader replaces it.
 */
class ENGINE_API FResourceCache : public FGCObject
{
public:
	[[nodiscard]] UStaticMesh* LoadStaticMesh(const FString& Path);
	[[nodiscard]] UTexture2D* LoadTexture(const FString& Path);
	[[nodiscard]] UTexture2D* CheckerTexture(int32 Size = 64);
	[[nodiscard]] UTexture2D* BumpNormalTexture(int32 Size = 256);

	/** Loads a .lmat material asset (cached by resolved path). On failure returns DefaultMaterial(). */
	[[nodiscard]] FMaterial LoadMaterial(const FString& Path);
	/** Grayscale checker template (Unreal-like default / WorldGrid placeholder). */
	[[nodiscard]] FMaterial DefaultMaterial();

	[[nodiscard]] UStaticMesh* GetCubeMesh();
	[[nodiscard]] UStaticMesh* GetPlaneMesh(float Size = 800.0f, float UvScale = 4.0f);
	[[nodiscard]] UStaticMesh* GetSphereMesh(int32 Segments = 24, int32 Rings = 16);

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

	/** Lets go of every asset (the next collection frees the ones nothing else references). */
	void Clear();
	/** Drops a cached material so the next LoadMaterial reloads it from disk. */
	void InvalidateMaterial(const FString& Path);

	// FGCObject
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override
	{
		return TEXT("FResourceCache");
	}

private:
	[[nodiscard]] static FString NormalizeKey(const FString& Path);
	[[nodiscard]] UStaticMesh* CacheMesh(const FString& Key, const FMeshData& Data);
	[[nodiscard]] UTexture2D* CacheTexture(const FString& Key, int32 SizeX, int32 SizeY, const uint8* Rgba);

	bool bTextureLoadingEnabled = true;
	TMap<FString, UStaticMesh*> Meshes;
	TMap<FString, UTexture2D*> Textures;
	TMap<FString, FMaterial> Materials;
};
