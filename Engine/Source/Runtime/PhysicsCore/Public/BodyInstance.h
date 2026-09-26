#pragma once

#include "CollisionQuery.h"
#include "CollisionResponseContainer.h"
#include "CoreMinimal.h"

enum class EBodyType : uint8
{
	Static,
	Dynamic,
};

/** How an FPhysScene body collides (Leon; UE keeps this in the body setup's aggregate geometry). */
enum class EBodyCollisionShape : uint8
{
	Box = 0, // AABB (BlockingVolume / dynamics / fallback)
	TriangleMesh = 1, // Static mesh ComplexAsSimple lite (CPU MeshData)
	/**
	 * A vertical capsule centred on the body's position (a capsule or sphere component): HalfExtents.X is its radius,
	 * HalfExtents.Z its half height with the caps. Queries test the capsule itself; the AABB-based parts of the scene
	 * (the side resolve, the floor support, the step, navigation) see its bounding box.
	 */
	Capsule = 2,
};

struct PHYSICSCORE_API FBodyInstanceDesc
{
	/**
	 * Who the body belongs to: the owning primitive component's unique id (UObject::GetUniqueID) for the bodies
	 * UPrimitiveComponent::CreatePhysicsState adds, any caller-chosen id otherwise. Traces report it and skip it.
	 */
	SIZE_T ComponentID = 0;
	EBodyType Type = EBodyType::Static;
	/** 0 = derive from the AABB volume when the component's shape is applied (FPhysScene::UpdateBodyFromComponent). */
	float Mass = 0.0f;
	/** UE-like Enable Gravity (only for Dynamic / simulatePhysics). */
	bool bEnableGravity = true;
};

/**
 * Physics-owned state (UE: FBodyInstance, which a component owns there). A component's body takes its shape and its
 * collision settings (object type, responses, query / physics) from the component when it is created; a simulated
 * body moves its component back after each step (FPhysScene::SyncComponentsToBodies), and a moved kinematic
 * component sends its body the new transform (UPrimitiveComponent::SendPhysicsTransform).
 */
struct PHYSICSCORE_API FBodyInstance
{
	SIZE_T ComponentID = 0;
	/** The unique id of the actor that owns the component (UE: the owner of OwnerComponent), for AddIgnoredActor. */
	SIZE_T OwnerID = NoComponentID;
	EBodyType Type = EBodyType::Static;
	EBodyCollisionShape CollisionShape = EBodyCollisionShape::Box;
	float Mass = 1.0f;
	bool bEnableGravity = true;
	/** Queries (traces, sweeps, the character's contacts) see the body (UE: ECollisionEnabled's query part). */
	bool bQueryEnabled = true;
	/** The body takes part in the simulation step (UE: ECollisionEnabled's physics part). */
	bool bPhysicsEnabled = true;
	/** What the body is (UE: ObjectType). */
	TEnumAsByte<ECollisionChannel> ObjectType = ECC_WorldStatic;
	/** How the body answers each trace channel (UE: CollisionResponses). */
	FCollisionResponseContainer CollisionResponses;
	FVector Position = FVector::ZeroVector;
	/** cm */
	FVector HalfExtents = FVector(50.0f);
	/** Horizontal velocity (X, Y), cm/s. */
	FVector2D VelXY = FVector2D::ZeroVector;
	/** Vertical velocity (Z), cm/s. */
	float VelocityZ = 0.0f;

	/**
	 * The collision a body added without a component starts with (FPhysScene::AddBody): a static body is a
	 * WorldStatic object that ignores ECC_WorldDynamic traces, a dynamic one a WorldDynamic object that ignores
	 * ECC_WorldStatic traces, and both block every other channel. This is the channel filter the scene applied before
	 * P17 (a trace on a world channel only saw bodies of that mobility); UE's default profiles block both.
	 */
	void SetDefaultCollision(EBodyType InType);

	/** The response to a trace channel, Ignore when queries do not see the body. */
	[[nodiscard]] ECollisionResponse GetResponseToChannel(ECollisionChannel Channel) const
	{
		return bQueryEnabled ? CollisionResponses.GetResponse(Channel) : ECR_Ignore;
	}
};
