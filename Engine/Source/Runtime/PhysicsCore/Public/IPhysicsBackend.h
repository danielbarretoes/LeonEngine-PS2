#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "TriangleCollision.h"


/// Physics backend contract. Implementations live under Plugins/Physics/*.
/// `FPhysScene` remains the gameplay-facing API; backends plug in behind it.
/// Arcade owns CMC side resolve / QuerySupportY; Jolt may own rigid Step + narrow-phase traces.
class IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    [[nodiscard]] virtual const char* GetName() const = 0;

    /// True when FPhysScene::Step should drive dynamics through this backend (Jolt).
    [[nodiscard]] virtual bool HasRigidWorld() const { return false; }

    /// True when FPhysScene Single/Multi traces should query this backend's narrow phase.
    [[nodiscard]] virtual bool HasNarrowPhaseTraces() const { return false; }

    virtual void RigidClear() {}

    /// Full rebuild after SyncFromLevel / Clear. Optional parallel triangle meshes for
    /// static ComplexAsSimple (Jolt MeshShape); boxes otherwise.
    virtual void RigidRebuild(
        const std::vector<FBodyInstance>& bodies,
        const std::vector<FTriangleMeshCollision>* triangleMeshes = nullptr,
        std::size_t skipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)()) {
        (void)bodies;
        (void)triangleMeshes;
        (void)skipLevelMeshIndex;
    }

    /// Before Step: push dynamic FBodyInstance state (e.g. CMC side push) without rebuilding.
    /// Creates/removes bodies only when `skipLevelMeshIndex` membership changes.
    virtual void RigidPrepareStep(
        const std::vector<FBodyInstance>& bodies,
        std::size_t skipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)()) {
        (void)bodies;
        (void)skipLevelMeshIndex;
    }

    /// Integrate with gravity magnitude along -Y; optional infinite floor at `floorY`.
    virtual void RigidStep(float deltaTime, float gravityMagnitude, float floorY) {
        (void)deltaTime;
        (void)gravityMagnitude;
        (void)floorY;
    }

    /// Write simulated COM positions / velocities back into dynamic BodyInstances.
    virtual void RigidReadBack(std::vector<FBodyInstance>& bodies) { (void)bodies; }

    /// Append body hits (not floor/slopes) sorted is caller's job. Returns true if any hit.
    virtual bool RigidLineTrace(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                const glm::vec3& end, ECollisionChannel channel,
                                std::size_t skipLevelMeshIndex) {
        (void)outHits;
        (void)start;
        (void)end;
        (void)channel;
        (void)skipLevelMeshIndex;
        return false;
    }

    virtual bool RigidSphereTrace(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                  const glm::vec3& end, float radius, ECollisionChannel channel,
                                  std::size_t skipLevelMeshIndex) {
        (void)outHits;
        (void)start;
        (void)end;
        (void)radius;
        (void)channel;
        (void)skipLevelMeshIndex;
        return false;
    }

    virtual bool RigidCapsuleTrace(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                   const glm::vec3& end, float radius, float halfHeight,
                                   ECollisionChannel channel, std::size_t skipLevelMeshIndex) {
        (void)outHits;
        (void)start;
        (void)end;
        (void)radius;
        (void)halfHeight;
        (void)channel;
        (void)skipLevelMeshIndex;
        return false;
    }
};

enum class EPhysicsBackendKind : std::uint8_t {
    Arcade = 0,
    Jolt = 1,
};

[[nodiscard]] std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(
    EPhysicsBackendKind kind = EPhysicsBackendKind::Arcade);

/// Plugins register extra backends at module startup (e.g. the JoltPhysics plugin registers Jolt).
/// CreatePhysicsBackend falls back to Arcade when no factory is registered for a kind.
using FPhysicsBackendFactory = std::unique_ptr<IPhysicsBackend> (*)();
void RegisterPhysicsBackendFactory(EPhysicsBackendKind kind, FPhysicsBackendFactory factory);

