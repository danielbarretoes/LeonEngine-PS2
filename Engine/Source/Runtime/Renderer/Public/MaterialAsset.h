#pragma once

#include "Material.h"

#include <nlohmann/json.hpp>

#include <string>

class FResourceCache;

/// True if JSON has surface fields (albedo, maps, …) — not gameplay-only keys.
[[nodiscard]] bool HasMaterialSurfaceFields(const nlohmann::json& Spec);

/// Apply material JSON fields onto an existing FMaterial (maps resolved via cache).
void PatchMaterialFromJson(FResourceCache& Resources, FMaterial& Material, const nlohmann::json& Spec);

/// Load a `.lmat` material asset.
/// Returns false on I/O / parse failure (leaves `out` unchanged).
[[nodiscard]] bool LoadMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out);

/// Engine default: grayscale checker (Unreal-like WorldGrid placeholder).
[[nodiscard]] FMaterial MakeDefaultCheckerMaterial(FResourceCache& Resources);
