#pragma once

#include <cstdint>
#include "Material.h"
#include "Vertex.h"
#include <string>
#include <vector>


/// Contiguous index range drawn with one material slot.
struct RENDERCORE_API FMeshSection {
    int IndexOffset = 0; // in indices (not bytes)
    int IndexCount = 0;
    int MaterialIndex = 0;
};

/// CPU-side mesh asset (no OpenGL handles).
struct RENDERCORE_API FMeshData {
    std::vector<FVertex> Vertices;
    std::vector<std::uint32_t> Indices;
    std::vector<FMeshSection> Submeshes;
    std::vector<FMaterial> Materials;
    /// Parallel to materials; resolved to FMaterial::albedoMap by FResourceCache.
    std::vector<std::string> AlbedoMapPaths;

    [[nodiscard]] bool empty() const { return Vertices.empty() || Indices.empty(); }
};

/// Orthonormalize tangents from triangle UVs (needed for normal mapping).
void ComputeTangents(FMeshData& Data);

