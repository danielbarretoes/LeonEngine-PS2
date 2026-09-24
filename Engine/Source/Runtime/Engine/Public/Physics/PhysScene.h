#pragma once

#include <cstddef>
#include "Engine/Level.h"
#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "IPhysicsBackend.h"
#include "PhysicsBackend.h"
#include "TriangleCollision.h"
#include <limits>
#include <memory>
#include <vector>


class FDebugDraw;

struct FCapsuleContactParams {
    float pushStrength = 1.0f;
    float stepUp = 0.35f;
    float skin = 0.02f;
    float walkBounds = 18.0f;
};

/// Inclined walkable/blocking surface for Arcade traces (CMC slope lite).
/// FPlane through `point` with unit `normal`, clipped by world AABB bounds.
struct FSlopePlane {
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 boundsCenter{0.0f};
    glm::vec3 boundsHalfExtents{1.0f, 1.0f, 1.0f};
};

struct FPhysSceneStepParams {
    float deltaTime = 0.0f;
    float damping = 6.0f;
    float walkBounds = 18.0f;
    /// World gravity for Dynamic bodies with enableGravity (Unreal Enable Gravity).
    float gravity = 24.0f;
    float floorY = 0.0f;
    float skin = 0.02f;
    /// Skip bodies whose levelMeshIndex matches (e.g. character visual if registered).
    std::size_t skipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)();
};

/// Lightweight XZ + arcade-Y physics scene (Unreal-style FPhysScene / FPhysScene).
/// Arcade: AABB (+ TriangleMesh statics) traces / CMC queries / optional arcade Step.
/// Jolt (`EPhysicsBackend::Jolt`, `LEON_WITH_JOLT`): rigid-body Step (incremental prepare;
/// MeshShape statics on rebuild) + Line/Sphere/Capsule narrow-phase traces; floor plane,
/// slope planes, and CMC side resolve stay Arcade.
/// Body owns position + AABB; Level is synced explicitly.
/// Implementation lives in Plugins/Physics/*; `IPhysicsBackend` is the swap seam.
class FPhysScene {
public:
    explicit FPhysScene(EPhysicsBackend backend = DefaultPhysicsBackend());

    [[nodiscard]] EPhysicsBackend GetBackend() const { return backend_; }
    [[nodiscard]] const IPhysicsBackend* GetBackendIface() const { return backendIface_.get(); }

    void Clear();
    std::size_t AddBody(const FBodyInstanceDesc& desc);

    /// Add an inclined plane clipped by a world AABB (for ramps / WalkableFloorZ tests).
    /// `pitchDegrees` around +Z: surface rises with +X; normal.y = cos(pitch).
    /// Optional `yawDegrees` rotates the rise direction in XZ (0 = +X).
    std::size_t AddSlopeRamp(const glm::vec3& boundsCenter, const glm::vec3& boundsHalfExtents,
                             float pitchDegrees, float yawDegrees = 0.0f);

    [[nodiscard]] const std::vector<FSlopePlane>& SlopePlanes() const { return slopePlanes_; }
    [[nodiscard]] std::vector<FSlopePlane>& SlopePlanes() { return slopePlanes_; }

    /// Pull position / half-extents / mass from StaticMeshComponent transforms.
    void SyncFromLevel(const Level& level);
    /// Write body positions back to StaticMeshComponent transforms.
    void SyncToLevel(Level& level) const;

    [[nodiscard]] const std::vector<FBodyInstance>& Bodies() const { return bodies_; }
    [[nodiscard]] std::vector<FBodyInstance>& Bodies() { return bodies_; }

    /// Parallel to Bodies(); empty / invalid when collisionShape is Box.
    [[nodiscard]] const std::vector<FTriangleMeshCollision>& TriangleMeshes() const {
        return triangleMeshes_;
    }
    [[nodiscard]] std::vector<FTriangleMeshCollision>& TriangleMeshes() { return triangleMeshes_; }

    [[nodiscard]] float QuerySupportY(const FCapsuleShape& capsule, const glm::vec3& feet,
                                      float floorY, float stepUp, float skin,
                                      std::size_t skipLevelMeshIndex) const;

    /// Unreal-like UWorld::LineTraceSingleByChannel against FPhysScene AABBs (+ optional floor).
    /// Pass `debugDraw` + `params.DrawDebugType = ForOneFrame` to visualize (F2 / gameplay).
    [[nodiscard]] bool LineTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                                const glm::vec3& end, ECollisionChannel channel,
                                                const FCollisionQueryParams& params = {},
                                                FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like UWorld::LineTraceMultiByChannel — all hits sorted nearest→farthest.
    [[nodiscard]] bool LineTraceMultiByChannel(std::vector<FHitResult>& outHits,
                                               const glm::vec3& start, const glm::vec3& end,
                                               ECollisionChannel channel,
                                               const FCollisionQueryParams& params = {},
                                               FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like UWorld::SphereTraceSingleByChannel (swept sphere ≈ expanded AABB).
    [[nodiscard]] bool SphereTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                                  const glm::vec3& end, float radius,
                                                  ECollisionChannel channel,
                                                  const FCollisionQueryParams& params = {},
                                                  FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like UWorld::SphereTraceMultiByChannel.
    [[nodiscard]] bool SphereTraceMultiByChannel(std::vector<FHitResult>& outHits,
                                                 const glm::vec3& start, const glm::vec3& end,
                                                 float radius, ECollisionChannel channel,
                                                 const FCollisionQueryParams& params = {},
                                                 FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like UWorld::CapsuleTraceSingleByChannel (`halfHeight` = cylinder half, excl. caps).
    [[nodiscard]] bool CapsuleTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                                   const glm::vec3& end, float radius,
                                                   float halfHeight, ECollisionChannel channel,
                                                   const FCollisionQueryParams& params = {},
                                                   FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like UWorld::CapsuleTraceMultiByChannel.
    [[nodiscard]] bool CapsuleTraceMultiByChannel(std::vector<FHitResult>& outHits,
                                                  const glm::vec3& start, const glm::vec3& end,
                                                  float radius, float halfHeight,
                                                  ECollisionChannel channel,
                                                  const FCollisionQueryParams& params = {},
                                                  FDebugDraw* debugDraw = nullptr) const;

    void ResolveCapsuleSides(const FCapsuleShape& capsule, glm::vec3& feet, const glm::vec2& wishXZ,
                             const FCapsuleContactParams& params, std::size_t skipLevelMeshIndex,
                             bool applyPush = true);

    /// Push a Dynamic body from a CMC capsule sweep hit (no penetration required).
    /// SafeMove stops at skin before ResolveCapsuleSides can see contact; call this on block hits.
    /// Returns true if a Dynamic body received velocity / contact shove.
    bool ApplyCapsuleSweepPush(std::size_t levelMeshIndex, const glm::vec2& wishXZ,
                               const glm::vec3& impactNormal, float pushStrength, float walkBounds);

    /// Integrate dynamic velocities + resolve body–body overlaps.
    void Step(const FPhysSceneStepParams& params);

    void AppendCollisionDebug(FDebugDraw& draw, const FCapsuleShape& capsule, const glm::vec3& feet,
                              std::size_t skipLevelMeshIndex) const;

    /// Body / triangle-mesh / slope wireframes only (editor Player Collision view mode).
    void AppendBodiesCollisionDebug(FDebugDraw& draw,
                                    std::size_t skipLevelMeshIndex =
                                        (std::numeric_limits<std::size_t>::max)()) const;

private:
    EPhysicsBackend backend_ = EPhysicsBackend::Arcade;
    /// Mutable: const Line/Sphere/Capsule traces may RigidPrepareStep so Jolt matches
    /// BodyInstances.
    mutable std::unique_ptr<IPhysicsBackend> backendIface_;
    std::vector<FBodyInstance> bodies_;
    std::vector<FTriangleMeshCollision> triangleMeshes_;
    std::vector<FSlopePlane> slopePlanes_;
};

