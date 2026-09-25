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
	/** Index of the level's static mesh the body mirrors (until levels hold actors, P13). */
	SIZE_T LevelMeshIndex = 0;
	EBodyType Type = EBodyType::Static;
	/** 0 = derive from AABB volume on SyncFromLevel. */
	float Mass = 0.0f;
	/** UE-like Enable Gravity (only for Dynamic / simulatePhysics). */
	bool bEnableGravity = true;
};

/** Physics-owned state. Level transforms are visuals; SyncFromLevel / SyncToLevel bridge them. */
struct PHYSICSCORE_API FBodyInstance
{
	SIZE_T LevelMeshIndex = 0;
	EBodyType Type = EBodyType::Static;
	EBodyCollisionShape CollisionShape = EBodyCollisionShape::Box;
	float Mass = 1.0f;
	bool bEnableGravity = true;
	FVector Position = FVector::ZeroVector;
	/** cm */
	FVector HalfExtents = FVector(50.0f);
	/** Horizontal velocity in the legacy Y-up world (X, Z), until P7. */
	FVector2D VelXz = FVector2D::ZeroVector;
	float VelocityY = 0.0f;
};
