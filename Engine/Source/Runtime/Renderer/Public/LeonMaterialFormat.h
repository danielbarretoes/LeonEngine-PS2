#pragma once

#include <glm/vec3.hpp>
#include "Material.h"
#include <string>


class FResourceCache;

/// Parsed `.lmat` for authoring (paths kept as strings; maps not required).
struct FLeonMaterialDocument {
    std::string name = "Material";
    FMaterial material{};
    std::string baseColorMapPath;
    std::string normalMapPath;
};

/// Unreal FMaterial Instance–like text asset (`.lmat`), not JSON / not `.uasset`.
/// Sections: [Info], [Parameters], [Textures]. See Docs/ASSET_FORMATS.md.
[[nodiscard]] bool IsLeonMaterialPath(const std::string& path);

/// Parse `.lmat` without resolving textures (FMaterial Editor).
[[nodiscard]] bool LoadLeonMaterialDocument(const std::string& path, FLeonMaterialDocument& out);

/// Parse `.lmat` text into a FMaterial (maps resolved via cache).
[[nodiscard]] bool LoadLeonMaterialFile(FResourceCache& resources, const std::string& path,
                                        FMaterial& out);

/// Write a `.lmat` from CPU material parameters (texture paths optional).
[[nodiscard]] bool SaveLeonMaterialFile(const std::string& path, const std::string& name,
                                        const FMaterial& material,
                                        const std::string& baseColorMapPath = {},
                                        const std::string& normalMapPath = {});

/// Default template text for a new solid-color material.
[[nodiscard]] std::string MakeDefaultLeonMaterialText(const std::string& name,
                                                      const glm::vec3& baseColor = {0.7f, 0.7f,
                                                                                    0.72f},
                                                      float metallic = 0.0f,
                                                      float roughness = 0.6f);

