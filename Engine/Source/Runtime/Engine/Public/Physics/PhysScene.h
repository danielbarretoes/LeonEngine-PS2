#pragma once

#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "IPhysicsBackend.h"
#include "PhysicsBackend.h"
#include "TriangleCollision.h"

class FDebugDraw;

struct ENGINE_API FCapsuleContactParams
{
	float PushStrength = 1.0f;
	/** cm */
	float StepUp = 35.0f;
	/** cm */
	float Skin = 2.0f;
	/** cm */
	float WalkBounds = 1800.0f;
};

/**
 * Inclined walkable / blocking surface for Arcade traces (CMC slope lite).
 * Plane through Point with the unit Normal, clipped by the world AABB bounds.
 */
struct ENGINE_API FSlopePlane
{
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector(0.0f, 1.0f, 0.0f);
	FVector BoundsCenter = FVector::ZeroVector;
	/** cm */
	FVector BoundsHalfExtents = FVector(100.0f, 100.0f, 100.0f);
};

struct ENGINE_API FPhysSceneStepParams
{
	float DeltaTime = 0.0f;
	float Damping = 6.0f;
	/** cm */
	float WalkBounds = 1800.0f;
	/** World gravity for Dynamic bodies with bEnableGravity (UE Enable Gravity), cm/s^2. */
	float Gravity = 2400.0f;
	float FloorY = 0.0f;
	/** cm */
	float Skin = 2.0f;
	/** Skip bodies whose LevelMeshIndex matches (e.g. the character visual if registered). */
	SIZE_T SkipLevelMeshIndex = NoLevelMeshIndex;
};

/**
 * Lightweight XZ + arcade-Y physics scene (UE-style FPhysScene), in centimetres, still Y up until P7 moves to Z up.
 * Arcade: AABB (+ TriangleMesh statics) traces / CMC queries / optional arcade Step.
 * Jolt (EPhysicsBackend::Jolt, the JoltPhysics plugin): rigid-body Step (incremental prepare; MeshShape statics on
 * rebuild) + Line / Sphere / Capsule narrow-phase traces; the floor plane, slope planes and the CMC side resolve
 * stay Arcade.
 * A body owns position + AABB; the level is synced explicitly. IPhysicsBackend is the swap seam.
 */
class ENGINE_API FPhysScene
{
public:
	explicit FPhysScene(EPhysicsBackend InBackend = DefaultPhysicsBackend());

	[[nodiscard]] EPhysicsBackend GetBackend() const
	{
		return Backend;
	}
	[[nodiscard]] const IPhysicsBackend* GetBackendIface() const
	{
		return BackendIface.Get();
	}

	void Clear();
	int32 AddBody(const FBodyInstanceDesc& Desc);

	/**
	 * Adds an inclined plane clipped by a world AABB (for ramps / WalkableFloorZ tests).
	 * PitchDegrees around +Z: the surface rises with +X; Normal.Y = cos(pitch).
	 * Optional YawDegrees rotates the rise direction in XZ (0 = +X).
	 */
	int32 AddSlopeRamp(
		const FVector& InBoundsCenter, const FVector& InBoundsHalfExtents, float PitchDegrees, float YawDegrees = 0.0f);

	[[nodiscard]] const TArray<FSlopePlane>& GetSlopePlanes() const
	{
		return SlopePlanes;
	}
	[[nodiscard]] TArray<FSlopePlane>& GetSlopePlanes()
	{
		return SlopePlanes;
	}

	/** Pulls position / half-extents / mass from the UStaticMeshComponent transforms. */
	void SyncFromLevel(const ULevel& Level);
	/** Writes body positions back to the UStaticMeshComponent transforms. */
	void SyncToLevel(ULevel& Level) const;

	[[nodiscard]] const TArray<FBodyInstance>& GetBodies() const
	{
		return Bodies;
	}
	[[nodiscard]] TArray<FBodyInstance>& GetBodies()
	{
		return Bodies;
	}

	/** Parallel to GetBodies(); empty / invalid when the body's collision shape is Box. */
	[[nodiscard]] const TArray<FTriangleMeshCollision>& GetTriangleMeshes() const
	{
		return TriangleMeshes;
	}
	[[nodiscard]] TArray<FTriangleMeshCollision>& GetTriangleMeshes()
	{
		return TriangleMeshes;
	}

	/** Highest walkable support under a capsule standing on Feet (FCollisionShape capsule). */
	[[nodiscard]] float QuerySupportY(const FCollisionShape& Capsule, const FVector& Feet, float InFloorY,
		float InStepUp, float InSkin, SIZE_T InSkipLevelMeshIndex) const;

	/**
	 * UE-like UWorld::LineTraceSingleByChannel against the FPhysScene AABBs (+ optional floor).
	 * Pass a DebugDraw and Params.DrawDebugType = ForOneFrame to visualize (F2 / gameplay).
	 */
	[[nodiscard]] bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, const FCollisionQueryParams& Params = {}, FDebugDraw* DebugDraw = nullptr) const;

	/** UE-like UWorld::LineTraceMultiByChannel: all hits sorted nearest to farthest. */
	[[nodiscard]] bool LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
		ECollisionChannel Channel, const FCollisionQueryParams& Params = {}, FDebugDraw* DebugDraw = nullptr) const;

	/** UE-like UWorld::SphereTraceSingleByChannel (swept sphere as an expanded AABB). */
	[[nodiscard]] bool SphereTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
		float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* DebugDraw = nullptr) const;

	/** UE-like UWorld::SphereTraceMultiByChannel. */
	[[nodiscard]] bool SphereTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
		float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* DebugDraw = nullptr) const;

	/** UE-like UWorld::CapsuleTraceSingleByChannel (HalfHeight = cylinder half, without the caps). */
	[[nodiscard]] bool CapsuleTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
		float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* DebugDraw = nullptr) const;

	/** UE-like UWorld::CapsuleTraceMultiByChannel. */
	[[nodiscard]] bool CapsuleTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
		float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params = {},
		FDebugDraw* DebugDraw = nullptr) const;

	void ResolveCapsuleSides(const FCollisionShape& Capsule, FVector& Feet, const FVector2D& WishXz,
		const FCapsuleContactParams& Params, SIZE_T InSkipLevelMeshIndex, bool bApplyPush = true);

	/**
	 * Pushes a Dynamic body from a CMC capsule sweep hit (no penetration required).
	 * SafeMove stops at skin before ResolveCapsuleSides can see contact; call this on block hits.
	 * Returns true if a Dynamic body received velocity / contact shove.
	 */
	bool ApplyCapsuleSweepPush(SIZE_T LevelMeshIndex, const FVector2D& WishXz, const FVector& ImpactNormal,
		float InPushStrength, float InWalkBounds);

	/** Integrates dynamic velocities and resolves body-body overlaps. */
	void Step(const FPhysSceneStepParams& Params);

	void AppendCollisionDebug(
		FDebugDraw& Draw, const FCollisionShape& Capsule, const FVector& Feet, SIZE_T InSkipLevelMeshIndex) const;

	/** Body / triangle-mesh / slope wireframes only (editor Player Collision view mode). */
	void AppendBodiesCollisionDebug(FDebugDraw& Draw, SIZE_T InSkipLevelMeshIndex = NoLevelMeshIndex) const;

private:
	EPhysicsBackend Backend = EPhysicsBackend::Arcade;
	/** Mutable: const Line / Sphere / Capsule traces may RigidPrepareStep so Jolt matches the body instances. */
	mutable TUniquePtr<IPhysicsBackend> BackendIface;
	TArray<FBodyInstance> Bodies;
	TArray<FTriangleMeshCollision> TriangleMeshes;
	TArray<FSlopePlane> SlopePlanes;
};
