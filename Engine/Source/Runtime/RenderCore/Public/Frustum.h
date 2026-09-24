#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>

namespace leon {

/// Axis-aligned bounding box in world space.
struct Aabb {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    /// Transform local AABB corners by `model` and re-wrap as a world AABB.
    [[nodiscard]] static Aabb fromLocalTransformed(const glm::vec3& localMin,
                                                   const glm::vec3& localMax,
                                                   const glm::mat4& model);

    /// Ray–AABB slab test. `dir` need not be unit length. Returns true if hit with t >= 0.
    [[nodiscard]] bool intersectRay(const glm::vec3& origin, const glm::vec3& dir,
                                    float& outT) const;
};

/// View-projection frustum as 6 planes (inside = n·x + d >= 0).
class Frustum {
public:
    void extractFromViewProjection(const glm::mat4& viewProjection);

    /// True if the AABB is at least partially inside the frustum.
    [[nodiscard]] bool intersectsAabb(const Aabb& box) const;

private:
    struct Plane {
        glm::vec3 normal{0.0f};
        float distance = 0.0f;
    };

    std::array<Plane, 6> planes_{};
};

} // namespace leon
