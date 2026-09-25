#pragma once

#include "BodyInstance.h"
#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "CoreMinimal.h"
#include "IPhysicsBackend.h"
#include "PhysicsBackend.h"
#include "TriangleCollision.h"

class FDebugDraw;
class UPrimitiveComponent;

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
	FVector Normal = FVector(0.0f, 0.0f, 1.0f);
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
	/** Height of the infinite floor plane (cm). */
	float FloorZ = 0.0f;
	/** cm */
	float Skin = 2.0f;
	/** Skip bodies whose ComponentID matches (e.g. the character visual if registered). */
	SIZE_T IgnoreComponentID = NoComponentID;
};

/**
 * Lightweight XY + arcade-Z physics scene (UE-style FPhysScene), in centimetres, Z up.
 * Arcade: AABB (+ TriangleMesh statics) traces / CMC queries / optional arcade Step.
 * Jolt (EPhysicsBackend::Jolt, the JoltPhysics plugin): rigid-body Step (incremental prepare; MeshShape statics on
 * rebuild) + Line / Sphere / Capsule narrow-phase traces; the floor plane, slope planes and the CMC side resolve
 * stay Arcade.
 * A body owns position + AABB. Components add theirs (UPrimitiveComponent::CreatePhysicsState → AddComponentBody) and
 * simulated ones follow them back (SyncComponentsToBodies); IPhysicsBackend is the swap seam.
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
	/** Adds a body with no owning component (tests, gameplay probes). Returns its index. */
	int32 AddBody(const FBodyInstanceDesc& Desc);

	/**
	 * Adds the body of a primitive component (UE: FBodyInstance::InitBody from CreatePhysicsState): its ComponentID is
	 * the component's unique id, its type follows IsSimulatingPhysics / IsGravityEnabled and its shape the component
	 * (UpdateBodyFromComponent). A rigid-body backend is rebuilt. Returns its index.
	 */
	int32 AddComponentBody(UPrimitiveComponent& Component);
	/** Removes a component's body (UE: FBodyInstance::TermBody from DestroyPhysicsState). */
	void RemoveComponentBody(const UPrimitiveComponent& Component);
	/** The component that owns a body; null for AddBody bodies and indices past the owned ones. */
	[[nodiscard]] UPrimitiveComponent* GetBodyOwner(int32 BodyIndex) const;

	/**
	 * Moves the component of every simulated body to the body's position after a step (UE:
	 * FPhysScene::SyncComponentsToBodies).
	 */
	void SyncComponentsToBodies() const;

	/** Rebuilds a rigid-body backend's world from the bodies and their triangle meshes (Jolt; nothing for Arcade). */
	void RebuildRigidWorld();

	/**
	 * Adds an inclined plane clipped by a world AABB (for ramps / WalkableFloorZ tests).
	 * The surface rises with +X at PitchDegrees; Normal.Z = cos(pitch).
	 * Optional YawDegrees turns the rise direction about Z (0 = +X, 90 = +Y, as a UE yaw).
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

	/**
	 * A body's shape from a component: a static mesh's world box (and its CPU triangles for a static body), a box
	 * component's scaled box, else a basic cube scaled like the component.
	 */
	void UpdateBodyFromComponent(int32 BodyIndex, const UPrimitiveComponent& Component);

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
	[[nodiscard]] float QuerySupportZ(const FCollisionShape& Capsule, const FVector& Feet, float InFloorZ,
		float InStepUp, float InSkin, SIZE_T InIgnoreComponentID) const;

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

	void ResolveCapsuleSides(const FCollisionShape& Capsule, FVector& Feet, const FVector2D& WishXY,
		const FCapsuleContactParams& Params, SIZE_T InIgnoreComponentID, bool bApplyPush = true);

	/**
	 * Pushes a Dynamic body from a CMC capsule sweep hit (no penetration required).
	 * SafeMove stops at skin before ResolveCapsuleSides can see contact; call this on block hits.
	 * Returns true if a Dynamic body received velocity / contact shove.
	 */
	bool ApplyCapsuleSweepPush(SIZE_T ComponentID, const FVector2D& WishXY, const FVector& ImpactNormal,
		float InPushStrength, float InWalkBounds);

	/** Integrates dynamic velocities and resolves body-body overlaps. */
	void Step(const FPhysSceneStepParams& Params);

	void AppendCollisionDebug(
		FDebugDraw& Draw, const FCollisionShape& Capsule, const FVector& Feet, SIZE_T InIgnoreComponentID) const;

	/** Body / triangle-mesh / slope wireframes only (editor Player Collision view mode). */
	void AppendBodiesCollisionDebug(FDebugDraw& Draw, SIZE_T InIgnoreComponentID = NoComponentID) const;

private:
	EPhysicsBackend Backend = EPhysicsBackend::Arcade;
	/** Mutable: const Line / Sphere / Capsule traces may RigidPrepareStep so Jolt matches the body instances. */
	mutable TUniquePtr<IPhysicsBackend> BackendIface;
	TArray<FBodyInstance> Bodies;
	TArray<FTriangleMeshCollision> TriangleMeshes;
	TArray<FSlopePlane> SlopePlanes;
	/**
	 * The component of each body added by AddComponentBody, parallel to Bodies (null for AddBody bodies). A component
	 * removes its body when it unregisters, so the pointers never outlive their components.
	 */
	TArray<UPrimitiveComponent*> BodyOwners;
};
