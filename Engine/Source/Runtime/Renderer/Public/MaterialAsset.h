#pragma once

#include "Material.h"
#include <nlohmann/json.hpp>
#include <string>

namespace leon {

class ResourceCache;

/// True if JSON has surface fields (albedo, maps, …) — not gameplay-only keys.
[[nodiscard]] bool HasMaterialSurfaceFields(const nlohmann::json& spec);

/// Apply material JSON fields onto an existing Material (maps resolved via cache).
void PatchMaterialFromJson(ResourceCache& resources, Material& material,
                           const nlohmann::json& spec);

/// Load a `.lmat` material asset.
/// Returns false on I/O / parse failure (leaves `out` unchanged).
[[nodiscard]] bool LoadMaterialFile(ResourceCache& resources, const std::string& path,
                                    Material& out);

/// Engine default: grayscale checker (Unreal-like WorldGrid placeholder).
[[nodiscard]] Material MakeDefaultCheckerMaterial(ResourceCache& resources);

} // namespace leon
