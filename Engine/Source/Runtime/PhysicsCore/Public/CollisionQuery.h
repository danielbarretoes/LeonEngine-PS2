#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

class FDebugDraw;

/// Unreal-like ECollisionChannel (micro-engine subset).
enum class ECollisionChannel : std::uint8_t
{
	WorldStatic, // Static PhysScene bodies
	WorldDynamic, // Dynamic PhysScene bodies
	Pawn, // Both (character / pawn queries)
	Visibility, // Both (generic line/sphere checks)
};

/// Unreal-like EDrawDebugTrace — draw the query for one frame when a FDebugDraw* is passed.
enum class EDrawDebugTrace : std::uint8_t
{
	None,
	ForOneFrame,
};

/// Unreal-like FHitResult for FPhysScene traces.
struct PHYSICSCORE_API FHitResult
{
	bool bBlockingHit = false;
	/// Normalized distance along [Start, End] in [0, 1].
	float Time = 1.0f;
	float Distance = 0.0f;
	/// World location of the sweep shape center at the blocking time (Unreal `Location`).
	glm::vec3 Location{0.0f};
	/// Surface contact point (Unreal `ImpactPoint`); equals Location for line traces.
	glm::vec3 ImpactPoint{0.0f};
	/// Unit normal pointing toward the trace start (away from the surface).
	glm::vec3 ImpactNormal{0.0f, 1.0f, 0.0f};
	glm::vec3 TraceStart{0.0f};
	glm::vec3 TraceEnd{0.0f};
	std::size_t LevelMeshIndex = (std::numeric_limits<std::size_t>::max)();
	/// True when the hit is the virtual infinite floor plane (FCollisionQueryParams).
	bool bFloorPlane = false;
};

/// Unreal-like FCollisionQueryParams.
struct PHYSICSCORE_API FCollisionQueryParams
{
	std::size_t SkipLevelMeshIndex = (std::numeric_limits<std::size_t>::max)();
	/// Include an infinite horizontal floor at FloorY (UCharacterMovementComponent floor).
	bool bTraceFloorPlane = false;
	float FloorY = 0.0f;
	/// When not None, FPhysScene traces draw into the provided FDebugDraw* (F2 / gameplay debug).
	EDrawDebugTrace DrawDebugType = EDrawDebugTrace::None;
};

/// Unreal-like DrawDebugLineTrace / Kismet System Library helpers (one frame into FDebugDraw).
void DrawDebugLineTrace(
	FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End, const std::vector<FHitResult>& Hits);
void DrawDebugSphereTrace(
	FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End, float Radius, const std::vector<FHitResult>& Hits);
void DrawDebugCapsuleTrace(FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End, float Radius,
	float HalfHeight, const std::vector<FHitResult>& Hits);
