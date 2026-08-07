#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace leon {

/// Baked walkable grid (Unreal NavMesh lite — no Recast). XZ cells + floor height.
struct NavMesh {
    float originX = 0.0f;
    float originZ = 0.0f;
    float cellSize = 0.5f;
    float floorY = 0.0f;
    int width = 0;
    int depth = 0;
    /// Row-major: index = iz * width + ix. true = walkable.
    std::vector<std::uint8_t> walkable;

    [[nodiscard]] bool IsValid() const { return width > 0 && depth > 0 && !walkable.empty(); }

    [[nodiscard]] bool InBounds(int ix, int iz) const {
        return ix >= 0 && iz >= 0 && ix < width && iz < depth;
    }

    [[nodiscard]] bool IsWalkable(int ix, int iz) const {
        return InBounds(ix, iz) && walkable[static_cast<std::size_t>(iz * width + ix)] != 0;
    }

    [[nodiscard]] glm::vec3 CellCenter(int ix, int iz) const {
        return {originX + (static_cast<float>(ix) + 0.5f) * cellSize, floorY,
                originZ + (static_cast<float>(iz) + 0.5f) * cellSize};
    }

    /// Nearest cell indices for a world XZ point (clamped). Returns false if mesh empty.
    [[nodiscard]] bool WorldToCell(float x, float z, int& outIx, int& outIz) const;
};

} // namespace leon
