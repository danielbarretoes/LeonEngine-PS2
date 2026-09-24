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

struct ENGINE_API FCapsuleContactParams {
    float PushStrength = 1.0f;
    float StepUp = 0.35f;
    float Skin = 0.02f;
    float WalkBounds = 18.0f;
};

/// Inclined walkable/blocking surface for Arcade traces (CMC slope lite).
/// FPlane through `point` with unit `normal`, clipped by world AABB bounds.
struct ENGINE_API FSlopePlane {
    glm::vec3 Point{0.0f};
    glm::vec3 Normal{0.0f, 1.0f, 0.0f};
    glm::vec3 BoundsCenter{0.0f};
    glm::vec3 BoundsHalfExtents{1.0f, 1.0f, 1.0f};
};

struct ENGINE_API FPhysSceneStepParams {
    float DeltaTime = 0.0f;
    float Damping = 6.0f;
    float WalkBounds = 18.0f;
    /// World gravity for Dynamic bodies with enableGravity (Unreal Enable Gravity).
    float Gravity = 24.0f;
    float FloorY = 0.0f;
    float Skin = 0.02f;
    /// Skip bodies whose levelMeshIndex matches (e.g. character visual if registered).
    std::size_t SkipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)();
};

/// Lightweight XZ + arcade-Y physics scene (Unreal-style FPhysScene / FPhysScene).
/// Arcade: AABB (+ TriangleMesh statics) traces / CMC queries / optional arcade Step.
/// Jolt (`EPhysicsBackend::Jolt`, `LEON_WITH_JOLT`): rigid-body Step (incremental prepare;
/// MeshShape statics on rebuild) + Line/Sphere/Capsule narrow-phase traces; floor plane,
/// slope planes, and CMC side resolve stay Arcade.
/// Body owns position + AABB; Level is synced explicitly.
/// Implementation lives in Plugins/Physics/*; `IPhysicsBackend` is the swap seam.
class ENGINE_API FPhysScene {
public:
    explicit FPhysScene(EPhysicsBackend InBackend = DefaultPhysicsBackend());

    [[nodiscard]] EPhysicsBackend GetBackend() const { return Backend; }
    [[nodiscard]] const IPhysicsBackend* GetBackendIface() const { return BackendIface.get(); }

    void Clear();
    std::size_t AddBody(const FBodyInstanceDesc& Desc);

    /// Add an inclined plane clipped by a world AABB (for ramps / WalkableFloorZ tests).
    /// `pitchDegrees` around +Z: surface rises with +X; normal.y = cos(pitch).
    /// Optional `yawDegrees` rotates the rise direction in XZ (0 = +X).
    std::size_t AddSlopeRamp(const glm::vec3& InBoundsCenter, const glm::vec3& InBoundsHalfExtents,
                             float PitchDegrees, float YawDegrees = 0.0f);

    [[nodiscard]] const std::vector<FSlopePlane>& GetSlopePlanes() const { return SlopePlanes; }
    [[nodiscard]] std::vector<FSlopePlane>& GetSlopePlanes() { return SlopePlanes; }

    /// Pull position / half-extents / mass from UStaticMeshComponent transforms.
    void SyncFromLevel(const ULevel& Level);
    /// Write body positions back to UStaticMeshComponent transforms.
    void SyncToLevel(ULevel& Level) const;

    [[nodiscard]] const std::vector<FBodyInstance>& GetBodies() const { return Bodies; }
    [[nodiscard]] std::vector<FBodyInstance>& GetBodies() { return Bodies; }

    /// Parallel to Bodies(); empty / invalid when collisionShape is Box.
    [[nodiscard]] const std::vector<FTriangleMeshCollision>& GetTriangleMeshes() const {
        return TriangleMeshes;
    }
    [[nodiscard]] std::vector<FTriangleMeshCollision>& GetTriangleMeshes() { return TriangleMeshes; }

    [[nodiscard]] float QuerySupportY(const FCapsuleShape& Capsule, const glm::vec3& Feet,
                                      float InFloorY, float InStepUp, float InSkin,
                                      std::size_t InSkipLevelMeshIndex) const;

    /// Unreal-like UWorld::LineTraceSingleByChannel against FPhysScene AABBs (+ optional floor).
    /// Pass `debugDraw` + `params.DrawDebugType = ForOneFrame` to visualize (F2 / gameplay).
    [[nodiscard]] bool LineTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                                const glm::vec3& End, ECollisionChannel Channel,
                                                const FCollisionQueryParams& Params = {},
                                                FDebugDraw* DebugDraw = nullptr) const;

    /// Unreal-like UWorld::LineTraceMultiByChannel — all hits sorted nearest→farthest.
    [[nodiscard]] bool LineTraceMultiByChannel(std::vector<FHitResult>& OutHits,
                                               const glm::vec3& Start, const glm::vec3& End,
                                               ECollisionChannel Channel,
                                               const FCollisionQueryParams& Params = {},
                                               FDebugDraw* DebugDraw = nullptr) const;

    /// Unreal-like UWorld::SphereTraceSingleByChannel (swept sphere ≈ expanded AABB).
    [[nodiscard]] bool SphereTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                                  const glm::vec3& End, float Radius,
                                                  ECollisionChannel Channel,
                                                  const FCollisionQueryParams& Params = {},
                                                  FDebugDraw* DebugDraw = nullptr) const;

    /// Unreal-like UWorld::SphereTraceMultiByChannel.
    [[nodiscard]] bool SphereTraceMultiByChannel(std::vector<FHitResult>& OutHits,
                                                 const glm::vec3& Start, const glm::vec3& End,
                                                 float Radius, ECollisionChannel Channel,
                                                 const FCollisionQueryParams& Params = {},
                                                 FDebugDraw* DebugDraw = nullptr) const;

    /// Unreal-like UWorld::CapsuleTraceSingleByChannel (`halfHeight` = cylinder half, excl. caps).
    [[nodiscard]] bool CapsuleTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                                   const glm::vec3& End, float Radius,
                                                   float HalfHeight, ECollisionChannel Channel,
                                                   const FCollisionQueryParams& Params = {},
                                                   FDebugDraw* DebugDraw = nullptr) const;

    /// Unreal-like UWorld::CapsuleTraceMultiByChannel.
    [[nodiscard]] bool CapsuleTraceMultiByChannel(std::vector<FHitResult>& OutHits,
                                                  const glm::vec3& Start, const glm::vec3& End,
                                                  float Radius, float HalfHeight,
                                                  ECollisionChannel Channel,
                                                  const FCollisionQueryParams& Params = {},
                                                  FDebugDraw* DebugDraw = nullptr) const;

    void ResolveCapsuleSides(const FCapsuleShape& Capsule, glm::vec3& Feet, const glm::vec2& WishXz,
                             const FCapsuleContactParams& Params, std::size_t InSkipLevelMeshIndex,
                             bool bApplyPush = true);

    /// Push a Dynamic body from a CMC capsule sweep hit (no penetration required).
    /// SafeMove stops at skin before ResolveCapsuleSides can see contact; call this on block hits.
    /// Returns true if a Dynamic body received velocity / contact shove.
    bool ApplyCapsuleSweepPush(std::size_t LevelMeshIndex, const glm::vec2& WishXz,
                               const glm::vec3& ImpactNormal, float InPushStrength, float InWalkBounds);

    /// Integrate dynamic velocities + resolve body–body overlaps.
    void Step(const FPhysSceneStepParams& Params);

    void AppendCollisionDebug(FDebugDraw& Draw, const FCapsuleShape& Capsule, const glm::vec3& Feet,
                              std::size_t InSkipLevelMeshIndex) const;

    /// Body / triangle-mesh / slope wireframes only (editor Player Collision view mode).
    void AppendBodiesCollisionDebug(FDebugDraw& Draw,
                                    std::size_t InSkipLevelMeshIndex =
                                        (std::numeric_limits<std::size_t>::max)()) const;

private:
    EPhysicsBackend Backend = EPhysicsBackend::Arcade;
    /// Mutable: const Line/Sphere/Capsule traces may RigidPrepareStep so Jolt matches
    /// BodyInstances.
    mutable std::unique_ptr<IPhysicsBackend> BackendIface;
    std::vector<FBodyInstance> Bodies;
    std::vector<FTriangleMeshCollision> TriangleMeshes;
    std::vector<FSlopePlane> SlopePlanes;
};

