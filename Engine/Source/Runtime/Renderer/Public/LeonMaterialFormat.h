#pragma once

#include <glm/vec3.hpp>
#include "Material.h"
#include <string>

namespace leon {

class ResourceCache;

/// Parsed `.lmat` for authoring (paths kept as strings; maps not required).
struct LeonMaterialDocument {
    std::string name = "Material";
    Material material{};
    std::string baseColorMapPath;
    std::string normalMapPath;
};

/// Unreal Material Instance–like text asset (`.lmat`), not JSON / not `.uasset`.
/// Sections: [Info], [Parameters], [Textures]. See Docs/ASSET_FORMATS.md.
[[nodiscard]] bool IsLeonMaterialPath(const std::string& path);

/// Parse `.lmat` without resolving textures (Material Editor).
[[nodiscard]] bool LoadLeonMaterialDocument(const std::string& path, LeonMaterialDocument& out);

/// Parse `.lmat` text into a Material (maps resolved via cache).
[[nodiscard]] bool LoadLeonMaterialFile(ResourceCache& resources, const std::string& path,
                                        Material& out);

/// Write a `.lmat` from CPU material parameters (texture paths optional).
[[nodiscard]] bool SaveLeonMaterialFile(const std::string& path, const std::string& name,
                                        const Material& material,
                                        const std::string& baseColorMapPath = {},
                                        const std::string& normalMapPath = {});

/// Default template text for a new solid-color material.
[[nodiscard]] std::string MakeDefaultLeonMaterialText(const std::string& name,
                                                      const glm::vec3& baseColor = {0.7f, 0.7f,
                                                                                    0.72f},
                                                      float metallic = 0.0f,
                                                      float roughness = 0.6f);

} // namespace leon
