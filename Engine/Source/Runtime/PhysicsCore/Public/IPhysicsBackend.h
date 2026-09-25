#pragma once

#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "TriangleCollision.h"

/**
 * Physics backend contract. Implementations live in Engine (Arcade) and in plugins (JoltPhysics).
 * FPhysScene remains the gameplay-facing API; backends plug in behind it.
 * Arcade owns the CMC side resolve / QuerySupportZ; Jolt may own rigid Step + narrow-phase traces.
 * Every length, velocity and acceleration crosses this interface in engine world units (cm, Z up); a backend with
 * other units or axes (Jolt: metres, Y up) converts at its own boundary.
 */
class PHYSICSCORE_API IPhysicsBackend
{
public:
	virtual ~IPhysicsBackend() = default;

	[[nodiscard]] virtual const TCHAR* GetName() const = 0;

	/** True when FPhysScene::Step should drive dynamics through this backend (Jolt). */
	[[nodiscard]] virtual bool HasRigidWorld() const
	{
		return false;
	}

	/** True when FPhysScene Single / Multi traces should query this backend's narrow phase. */
	[[nodiscard]] virtual bool HasNarrowPhaseTraces() const
	{
		return false;
	}

	virtual void RigidClear()
	{
	}

	/**
	 * Full rebuild after the bodies changed (FPhysScene::RebuildRigidWorld) / Clear. Optional parallel triangle meshes
	 * for static ComplexAsSimple (Jolt MeshShape); boxes otherwise.
	 */
	virtual void RigidRebuild(const TArray<FBodyInstance>& Bodies,
		const TArray<FTriangleMeshCollision>* TriangleMeshes = nullptr, SIZE_T IgnoreComponentID = NoComponentID)
	{
		(void)Bodies;
		(void)TriangleMeshes;
		(void)IgnoreComponentID;
	}

	/**
	 * Before Step: push dynamic FBodyInstance state (e.g. CMC side push) without rebuilding. Creates / removes bodies
	 * only when IgnoreComponentID membership changes.
	 */
	virtual void RigidPrepareStep(const TArray<FBodyInstance>& Bodies, SIZE_T IgnoreComponentID = NoComponentID)
	{
		(void)Bodies;
		(void)IgnoreComponentID;
	}

	/** Integrate with gravity magnitude along -Z; optional infinite floor at height FloorZ. */
	virtual void RigidStep(float DeltaTime, float GravityMagnitude, float FloorZ)
	{
		(void)DeltaTime;
		(void)GravityMagnitude;
		(void)FloorZ;
	}

	/** Write simulated COM positions / velocities back into dynamic body instances. */
	virtual void RigidReadBack(TArray<FBodyInstance>& Bodies)
	{
		(void)Bodies;
	}

	/** Appends body hits (not floor / slopes); sorting is the caller's job. Returns true if any hit. */
	virtual bool RigidLineTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, SIZE_T IgnoreComponentID)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Channel;
		(void)IgnoreComponentID;
		return false;
	}

	virtual bool RigidSphereTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
		ECollisionChannel Channel, SIZE_T IgnoreComponentID)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Radius;
		(void)Channel;
		(void)IgnoreComponentID;
		return false;
	}

	virtual bool RigidCapsuleTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
		float HalfHeight, ECollisionChannel Channel, SIZE_T IgnoreComponentID)
	{
		(void)OutHits;
		(void)Start;
		(void)End;
		(void)Radius;
		(void)HalfHeight;
		(void)Channel;
		(void)IgnoreComponentID;
		return false;
	}
};

enum class EPhysicsBackendKind : uint8
{
	Arcade = 0,
	Jolt = 1,
};

[[nodiscard]] TUniquePtr<IPhysicsBackend> CreatePhysicsBackend(EPhysicsBackendKind Kind = EPhysicsBackendKind::Arcade);

/**
 * Plugins register extra backends at module startup (e.g. the JoltPhysics plugin registers Jolt).
 * CreatePhysicsBackend falls back to Arcade when no factory is registered for a kind.
 */
using FPhysicsBackendFactory = TUniquePtr<IPhysicsBackend> (*)();
void RegisterPhysicsBackendFactory(EPhysicsBackendKind Kind, FPhysicsBackendFactory Factory);
