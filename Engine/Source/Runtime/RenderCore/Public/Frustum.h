#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>


/// Axis-aligned bounding box in world space.
struct FBox {
    glm::vec3 Min{0.0f};
    glm::vec3 Max{0.0f};

    /// FTransform local AABB corners by `model` and re-wrap as a world AABB.
    [[nodiscard]] static FBox FromLocalTransformed(const glm::vec3& LocalMin,
                                                   const glm::vec3& LocalMax,
                                                   const glm::mat4& Model);

    /// Ray–AABB slab test. `dir` need not be unit length. Returns true if hit with t >= 0.
    [[nodiscard]] bool IntersectRay(const glm::vec3& Origin, const glm::vec3& Dir,
                                    float& OutT) const;
};

/// View-projection frustum as 6 planes (inside = n·x + d >= 0).
class FFrustum {
public:
    void ExtractFromViewProjection(const glm::mat4& ViewProjection);

    /// True if the AABB is at least partially inside the frustum.
    [[nodiscard]] bool IntersectsAabb(const FBox& Box) const;

private:
    struct FPlane {
        glm::vec3 Normal{0.0f};
        float Distance = 0.0f;
    };

    std::array<FPlane, 6> Planes{};
};

