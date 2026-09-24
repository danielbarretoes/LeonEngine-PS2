#pragma once

#include <cstdint>
#include "Material.h"
#include "Vertex.h"
#include <string>
#include <vector>


/// Contiguous index range drawn with one material slot.
struct FMeshSection {
    int indexOffset = 0; // in indices (not bytes)
    int indexCount = 0;
    int materialIndex = 0;
};

/// CPU-side mesh asset (no OpenGL handles).
struct FMeshData {
    std::vector<FVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<FMeshSection> submeshes;
    std::vector<FMaterial> materials;
    /// Parallel to materials; resolved to FMaterial::albedoMap by FResourceCache.
    std::vector<std::string> albedoMapPaths;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

/// Orthonormalize tangents from triangle UVs (needed for normal mapping).
void ComputeTangents(FMeshData& data);

