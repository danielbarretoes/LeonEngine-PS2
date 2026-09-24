#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>

enum class EBodyType : std::uint8_t
{
	Static,
	Dynamic,
};

/// Unreal-like collision representation for a FPhysScene body.
enum class ECollisionShape : std::uint8_t
{
	Box = 0, // AABB (BlockingVolume / dynamics / fallback)
	TriangleMesh = 1, // Static mesh ComplexAsSimple lite (CPU MeshData)
};

struct PHYSICSCORE_API FBodyInstanceDesc
{
	std::size_t LevelMeshIndex = 0;
	EBodyType Type = EBodyType::Static;
	/// 0 = derive from AABB volume on SyncFromLevel.
	float Mass = 0.0f;
	/// Unreal-like Enable Gravity (only for Dynamic / simulatePhysics).
	bool bEnableGravity = true;
};

/// Physics-owned state. Level transforms are visuals; SyncFromLevel / SyncToLevel bridge them.
struct PHYSICSCORE_API FBodyInstance
{
	std::size_t LevelMeshIndex = 0;
	EBodyType Type = EBodyType::Static;
	ECollisionShape CollisionShape = ECollisionShape::Box;
	float Mass = 1.0f;
	bool bEnableGravity = true;
	glm::vec3 Position{0.0f};
	glm::vec3 HalfExtents{0.5f};
	glm::vec2 VelXz{0.0f};
	float VelocityY = 0.0f;
};
