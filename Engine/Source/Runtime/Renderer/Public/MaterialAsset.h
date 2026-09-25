#pragma once

#include "CoreMinimal.h"
#include "Material.h"

class FJsonObject;
class FResourceCache;

/** True if the JSON object has surface fields (albedo, maps, ...), not only gameplay keys. */
[[nodiscard]] RENDERER_API bool HasMaterialSurfaceFields(const FJsonObject& Spec);

/** Applies material JSON fields onto an existing FMaterial (maps resolved via the cache). */
RENDERER_API void PatchMaterialFromJson(FResourceCache& Resources, FMaterial& Material, const FJsonObject& Spec);

/** Loads a .lmat material asset. Returns false on I/O / parse failure (leaves Out unchanged). */
[[nodiscard]] RENDERER_API bool LoadMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out);

/** Engine default: grayscale checker (Unreal-like WorldGrid placeholder). */
[[nodiscard]] RENDERER_API FMaterial MakeDefaultCheckerMaterial(FResourceCache& Resources);
