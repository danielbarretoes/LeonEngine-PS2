#pragma once

#include "Material.h"
#include <nlohmann/json.hpp>
#include <string>


class FResourceCache;

/// True if JSON has surface fields (albedo, maps, …) — not gameplay-only keys.
[[nodiscard]] bool HasMaterialSurfaceFields(const nlohmann::json& spec);

/// Apply material JSON fields onto an existing FMaterial (maps resolved via cache).
void PatchMaterialFromJson(FResourceCache& resources, FMaterial& material,
                           const nlohmann::json& spec);

/// Load a `.lmat` material asset.
/// Returns false on I/O / parse failure (leaves `out` unchanged).
[[nodiscard]] bool LoadMaterialFile(FResourceCache& resources, const std::string& path,
                                    FMaterial& out);

/// Engine default: grayscale checker (Unreal-like WorldGrid placeholder).
[[nodiscard]] FMaterial MakeDefaultCheckerMaterial(FResourceCache& resources);

