#pragma once

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
 * Physics-owned state (UE: FBodyInstance, which a component owns there). A component's body takes its shape from the
 * component when it is created; a simulated body moves its component back after each step
 * (FPhysScene::SyncComponentsToBodies).
 */
struct PHYSICSCORE_API FBodyInstance
{
	SIZE_T ComponentID = 0;
	EBodyType Type = EBodyType::Static;
	EBodyCollisionShape CollisionShape = EBodyCollisionShape::Box;
	float Mass = 1.0f;
	bool bEnableGravity = true;
	FVector Position = FVector::ZeroVector;
	/** cm */
	FVector HalfExtents = FVector(50.0f);
	/** Horizontal velocity (X, Y), cm/s. */
	FVector2D VelXY = FVector2D::ZeroVector;
	/** Vertical velocity (Z), cm/s. */
	float VelocityZ = 0.0f;
};
