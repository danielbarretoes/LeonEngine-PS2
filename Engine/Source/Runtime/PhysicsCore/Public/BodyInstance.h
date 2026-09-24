#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>

namespace leon {

enum class EBodyType : std::uint8_t {
    Static,
    Dynamic,
};

/// Unreal-like collision representation for a PhysScene body.
enum class ECollisionShape : std::uint8_t {
    Box = 0,          // AABB (BlockingVolume / dynamics / fallback)
    TriangleMesh = 1, // Static mesh ComplexAsSimple lite (CPU MeshData)
};

struct BodyInstanceDesc {
    std::size_t levelMeshIndex = 0;
    EBodyType type = EBodyType::Static;
    /// 0 = derive from AABB volume on SyncFromLevel.
    float mass = 0.0f;
    /// Unreal-like Enable Gravity (only for Dynamic / simulatePhysics).
    bool enableGravity = true;
};

/// Physics-owned state. Level transforms are visuals; SyncFromLevel / SyncToLevel bridge them.
struct BodyInstance {
    std::size_t levelMeshIndex = 0;
    EBodyType type = EBodyType::Static;
    ECollisionShape collisionShape = ECollisionShape::Box;
    float mass = 1.0f;
    bool enableGravity = true;
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::vec2 velXZ{0.0f};
    float velocityY = 0.0f;
};

} // namespace leon
