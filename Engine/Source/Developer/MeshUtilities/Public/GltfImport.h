#pragma once

#include "MeshData.h"
#include <string>
#include <vector>


struct MESHUTILITIES_API FGltfImportedMaterial {
    std::string Name;
    std::string LmatRelativePath; // path written relative to out directory
};

/// Load first mesh (all primitives merged) from `.gltf` / `.glb` into FMeshData.
/// Optionally writes `.lmat` (+ copies textures) under `materialsOutDir` when non-empty.
/// Edit-time / cook only — not part of shipping `leon_engine`.
[[nodiscard]] bool LoadStaticMeshFromGltf(const std::string& Path, FMeshData& Out,
                                          const std::string& MaterialsOutDir,
                                          std::vector<FGltfImportedMaterial>* OutMaterials,
                                          std::string& OutError);

