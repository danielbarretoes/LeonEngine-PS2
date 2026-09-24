#pragma once

#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "TriangleCollision.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

/// Physics backend contract. Implementations live under Plugins/Physics/*.
/// `FPhysScene` remains the gameplay-facing API; backends plug in behind it.
/// Arcade owns CMC side resolve / QuerySupportY; Jolt may own rigid Step + narrow-phase traces.
class PHYSICSCORE_API IPhysicsBackend
{
public:
	virtual ~IPhysicsBackend() = default;

	[[nodiscard]] virtual const char* GetName() const = 0;

	/// True when FPhysScene::Step should drive dynamics through this backend (Jolt).
	[[nodiscard]] virtual bool HasRigidWorld() const
	{
		return false;
	}

	/// True when FPhysScene Single/Multi traces should query this backend's narrow phase.
	[[nodiscard]] virtual bool HasNarrowPhaseTraces() const
	{
		return false;
	}

	virtual void RigidClear()
	{
	}

	/// Full rebuild after SyncFromLevel / Clear. Optional parallel triangle meshes for
	/// static ComplexAsSimple (Jolt MeshShape); boxes otherwise.
	virtual void RigidRebuild(const std::vector<FBodyInstance>& Bodies,
		const std::vector<FTriangleMeshCollision>* TriangleMeshes = nullptr,
		std::size_t SkipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)())
	{
		(void)Bodies;
		(void)TriangleMeshes;
		(void)SkipLevelMeshIndex;
	}

	/// Before Step: push dynamic FBodyInstance state (e.g. CMC side push) without rebuilding.
	/// Creates/removes bodies only when `skipLevelMeshIndex` membership changes.
	virtual void RigidPrepareStep(const std::vector<FBodyInstance>& Bodies,
		std::size_t SkipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)())
	{
		(void)Bodies;
		(void)SkipLevelMeshIndex;
	}

	/// Integrate with gravity magnitude along -Y; optional infinite floor at `floorY`.
	virtual void RigidStep(float DeltaTime, float GravityMagnitude, float FloorY)
	{
		(void)DeltaTime;
		(void)GravityMagnitude;
		(void)FloorY;
	}

	/// Write simulated COM positions / velocities back into dynamic BodyInstances.
	virtual void RigidReadBack(std::vector<FBodyInstance>& Bodies)
	{
		(void)Bodies;
	}

	/// Append body hits (not floor/slopes) sorted is caller's job. Returns true if any hit.
	virtual bool RigidLineTrace(std::vector<FHitResult>& OutHits, const glm::vec3& Start, const glm::vec3& End,
		ECollisionChannel Channel, std::size_t SkipLevelMeshIndex)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Channel;
		(void)SkipLevelMeshIndex;
		return false;
	}

	virtual bool RigidSphereTrace(std::vector<FHitResult>& OutHits, const glm::vec3& Start, const glm::vec3& End,
		float Radius, ECollisionChannel Channel, std::size_t SkipLevelMeshIndex)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Radius;
		(void)Channel;
		(void)SkipLevelMeshIndex;
		return false;
	}

	virtual bool RigidCapsuleTrace(std::vector<FHitResult>& OutHits, const glm::vec3& Start, const glm::vec3& End,
		float Radius, float HalfHeight, ECollisionChannel Channel, std::size_t SkipLevelMeshIndex)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Radius;
		(void)HalfHeight;
		(void)Channel;
		(void)SkipLevelMeshIndex;
		return false;
	}
};

enum class EPhysicsBackendKind : std::uint8_t
{
	Arcade = 0,
	Jolt = 1,
};

[[nodiscard]] std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(
	EPhysicsBackendKind Kind = EPhysicsBackendKind::Arcade);

/// Plugins register extra backends at module startup (e.g. the JoltPhysics plugin registers Jolt).
/// CreatePhysicsBackend falls back to Arcade when no factory is registered for a kind.
using FPhysicsBackendFactory = std::unique_ptr<IPhysicsBackend> (*)();
void RegisterPhysicsBackendFactory(EPhysicsBackendKind Kind, FPhysicsBackendFactory Factory);
