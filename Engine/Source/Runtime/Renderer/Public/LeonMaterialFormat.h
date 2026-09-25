#pragma once

#include "CoreMinimal.h"
#include "Material.h"

class FResourceCache;

/** Parsed .lmat for authoring (paths kept as strings; maps not required). */
struct RENDERER_API FLeonMaterialDocument
{
	FString Name = "Material";
	FMaterial Material{};
	FString BaseColorMapPath;
	FString NormalMapPath;
};

/**
 * Unreal Material Instance-like text asset (.lmat), not JSON / not .uasset.
 * Sections: [Info], [Parameters], [Textures]. See Docs/ASSET_FORMATS.md.
 */
[[nodiscard]] RENDERER_API bool IsLeonMaterialPath(const FString& Path);

/** Parses a .lmat without resolving textures (material editor). */
[[nodiscard]] RENDERER_API bool LoadLeonMaterialDocument(const FString& Path, FLeonMaterialDocument& Out);

/** Parses .lmat text into a FMaterial (maps resolved via the cache). */
[[nodiscard]] RENDERER_API bool LoadLeonMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out);

/** Writes a .lmat from CPU material parameters (texture paths optional). */
[[nodiscard]] RENDERER_API bool SaveLeonMaterialFile(const FString& Path, const FString& InName,
	const FMaterial& InMaterial, const FString& InBaseColorMapPath = FString(),
	const FString& InNormalMapPath = FString());

/** Default template text for a new solid-color material. */
[[nodiscard]] RENDERER_API FString MakeDefaultLeonMaterialText(const FString& InName,
	const FVector& BaseColor = FVector(0.7f, 0.7f, 0.72f), float Metallic = 0.0f, float Roughness = 0.6f);
