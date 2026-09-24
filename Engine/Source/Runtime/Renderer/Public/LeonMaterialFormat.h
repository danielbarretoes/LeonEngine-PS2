#pragma once

#include <glm/vec3.hpp>
#include "Material.h"
#include <string>


class FResourceCache;

/// Parsed `.lmat` for authoring (paths kept as strings; maps not required).
struct FLeonMaterialDocument {
    std::string Name = "Material";
    FMaterial Material{};
    std::string BaseColorMapPath;
    std::string NormalMapPath;
};

/// Unreal FMaterial Instance–like text asset (`.lmat`), not JSON / not `.uasset`.
/// Sections: [Info], [Parameters], [Textures]. See Docs/ASSET_FORMATS.md.
[[nodiscard]] bool IsLeonMaterialPath(const std::string& Path);

/// Parse `.lmat` without resolving textures (FMaterial Editor).
[[nodiscard]] bool LoadLeonMaterialDocument(const std::string& Path, FLeonMaterialDocument& Out);

/// Parse `.lmat` text into a FMaterial (maps resolved via cache).
[[nodiscard]] bool LoadLeonMaterialFile(FResourceCache& Resources, const std::string& Path,
                                        FMaterial& Out);

/// Write a `.lmat` from CPU material parameters (texture paths optional).
[[nodiscard]] bool SaveLeonMaterialFile(const std::string& Path, const std::string& InName,
                                        const FMaterial& InMaterial,
                                        const std::string& InBaseColorMapPath = {},
                                        const std::string& InNormalMapPath = {});

/// Default template text for a new solid-color material.
[[nodiscard]] std::string MakeDefaultLeonMaterialText(const std::string& InName,
                                                      const glm::vec3& BaseColor = {0.7f, 0.7f,
                                                                                    0.72f},
                                                      float Metallic = 0.0f,
                                                      float Roughness = 0.6f);

