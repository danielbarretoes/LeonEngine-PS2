#pragma once

#include <leon/render/MeshData.h>
#include <string>
#include <vector>

namespace leon {

struct GltfImportedMaterial {
    std::string name;
    std::string lmatRelativePath; // path written relative to out directory
};

/// Load first mesh (all primitives merged) from `.gltf` / `.glb` into MeshData.
/// Optionally writes `.lmat` (+ copies textures) under `materialsOutDir` when non-empty.
/// Edit-time / cook only — not part of shipping `leon_engine`.
[[nodiscard]] bool LoadStaticMeshFromGltf(const std::string& path, MeshData& out,
                                          const std::string& materialsOutDir,
                                          std::vector<GltfImportedMaterial>* outMaterials,
                                          std::string& outError);

} // namespace leon
