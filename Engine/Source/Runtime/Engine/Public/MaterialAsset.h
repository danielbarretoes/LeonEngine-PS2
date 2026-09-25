#pragma once

#include "CoreMinimal.h"
#include "Material.h"

class FJsonObject;
class FResourceCache;

/** True if the JSON object has surface fields (albedo, maps, ...), not only gameplay keys. */
[[nodiscard]] ENGINE_API bool HasMaterialSurfaceFields(const FJsonObject& Spec);

/** Applies material JSON fields onto an existing FMaterial (maps resolved via the cache). */
ENGINE_API void PatchMaterialFromJson(FResourceCache& Resources, FMaterial& Material, const FJsonObject& Spec);

/**
 * Parses .lmat text into a FMaterial (LeonMaterialFormat.h) and loads its maps through the cache ("checker" and
 * "bump" are the built-in procedural maps).
 */
[[nodiscard]] ENGINE_API bool LoadLeonMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out);

/** Loads a .lmat material asset. Returns false on I/O / parse failure (leaves Out unchanged). */
[[nodiscard]] ENGINE_API bool LoadMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out);

/** Engine default: grayscale checker (Unreal-like WorldGrid placeholder). */
[[nodiscard]] ENGINE_API FMaterial MakeDefaultCheckerMaterial(FResourceCache& Resources);
