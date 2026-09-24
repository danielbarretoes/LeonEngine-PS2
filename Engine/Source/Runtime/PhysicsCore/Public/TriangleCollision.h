#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <vector>

namespace leon {

/// Baked world-space triangle mesh for static ComplexAsSimple lite (Arcade traces /
/// QuerySupportY; Jolt MeshShape on rebuild).
struct TriangleMeshCollision {
    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] bool IsValid() const {
        return !positions.empty() && indices.size() >= 3 && (indices.size() % 3) == 0;
    }

    void Clear() {
        positions.clear();
        indices.clear();
    }
};

/// Segment [start,end] vs triangle. `t` in [0,1] along the segment. Outward/front normal.
[[nodiscard]] bool SegmentTriangle(const glm::vec3& start, const glm::vec3& end, const glm::vec3& v0,
                                   const glm::vec3& v1, const glm::vec3& v2, float& outT,
                                   glm::vec3& outNormal);

/// Sphere/capsule approximation: intersect against the plane offset by `inflate` along the
/// triangle normal (same idea as slope inflate). Hit only if the impact projects inside the tri.
[[nodiscard]] bool SegmentTriangleInflated(const glm::vec3& start, const glm::vec3& end,
                                           const glm::vec3& v0, const glm::vec3& v1,
                                           const glm::vec3& v2, float inflate, float& outT,
                                           glm::vec3& outNormal);

/// Nearest segment hit against a triangle mesh (optional inflate for sphere/capsule).
[[nodiscard]] bool SegmentTriangleMesh(const glm::vec3& start, const glm::vec3& end,
                                       const TriangleMeshCollision& mesh, float inflate,
                                       float& outT, glm::vec3& outNormal);

} // namespace leon
