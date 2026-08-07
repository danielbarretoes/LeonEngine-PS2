#pragma once

#include <cstdint>
#include <leon/render/Material.h>
#include <leon/render/Vertex.h>
#include <string>
#include <vector>

namespace leon {

/// Contiguous index range drawn with one material slot.
struct SubMesh {
    int indexOffset = 0; // in indices (not bytes)
    int indexCount = 0;
    int materialIndex = 0;
};

/// CPU-side mesh asset (no OpenGL handles).
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SubMesh> submeshes;
    std::vector<Material> materials;
    /// Parallel to materials; resolved to Material::albedoMap by ResourceCache.
    std::vector<std::string> albedoMapPaths;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

/// Orthonormalize tangents from triangle UVs (needed for normal mapping).
void ComputeTangents(MeshData& data);

} // namespace leon
