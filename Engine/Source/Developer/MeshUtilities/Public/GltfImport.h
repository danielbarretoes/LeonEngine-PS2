#pragma once

#include "MeshData.h"
#include <string>
#include <vector>


struct FGltfImportedMaterial {
    std::string name;
    std::string lmatRelativePath; // path written relative to out directory
};

/// Load first mesh (all primitives merged) from `.gltf` / `.glb` into FMeshData.
/// Optionally writes `.lmat` (+ copies textures) under `materialsOutDir` when non-empty.
/// Edit-time / cook only — not part of shipping `leon_engine`.
[[nodiscard]] bool LoadStaticMeshFromGltf(const std::string& path, FMeshData& out,
                                          const std::string& materialsOutDir,
                                          std::vector<FGltfImportedMaterial>* outMaterials,
                                          std::string& outError);

