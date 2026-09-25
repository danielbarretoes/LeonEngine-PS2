#pragma once

#include "AI/Navigation/NavMesh.h"
#include "CoreMinimal.h"

class ULevel;
class FPhysScene;
class FDebugDraw;

/** Level mesh tags recognized by UNavigationSystem bake (Unreal NavArea-style hints). */
namespace NavTags
{
	inline constexpr const TCHAR* Blocker = "NavBlocker";
	inline constexpr const TCHAR* Walkable = "NavWalkable";
} // namespace NavTags

/**
 * Unreal-like UNavigationSystem lite: bake a grid NavMesh from static FPhysScene bodies,
 * then FindPath for AAIController. Not Recast/Detour — swap-compatible later.
 */
class ENGINE_API UNavigationSystem
{
public:
	/** Cell size / agent radius used when baking, in cm, at least 5 (defaults ~ character capsule radius). */
	void SetCellSize(float InCellSize)
	{
		CellSize = InCellSize > MinBakeSize ? InCellSize : MinBakeSize;
	}
	void SetAgentRadius(float InAgentRadius)
	{
		AgentRadius = InAgentRadius > MinBakeSize ? InAgentRadius : MinBakeSize;
	}
	[[nodiscard]] float GetCellSize() const
	{
		return CellSize;
	}
	[[nodiscard]] float GetAgentRadius() const
	{
		return AgentRadius;
	}

	/**
	 * Bake walkable grid from static box bodies. Wide/flat floor slabs stay walkable.
	 * walkBounds is half-extent from origin on XZ (matches UCharacterMovementComponent::WalkBounds).
	 */
	void BuildFromPhysScene(const FPhysScene& Physics, float FloorY, float WalkBounds);

	/** Prefer this: skips Plane; honors NavTags::Blocker / NavTags::Walkable on meshes. */
	void BuildFromLevel(const ULevel& Level, const FPhysScene& Physics, float FloorY, float WalkBounds);

	void Clear();

	[[nodiscard]] bool HasNavMesh() const
	{
		return Mesh.IsValid();
	}
	[[nodiscard]] const FNavMesh& GetNavMesh() const
	{
		return Mesh;
	}
	[[nodiscard]] int GetBlockerCount() const
	{
		return BlockerCount;
	}
	[[nodiscard]] int GetWalkableCellCount() const
	{
		return WalkableCellCount;
	}

	/** Snap to nearest walkable cell center (Y = floorY). Returns false if no nav mesh. */
	[[nodiscard]] bool ProjectPointToNavigation(const FVector& World, FVector& OutProjected) const;

	/**
	 * A* on the grid (8-connected). Fills outPath with world waypoints (includes end).
	 * Returns false if no path; outPath cleared on failure.
	 */
	[[nodiscard]] bool FindPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const;

	/** Draw walkable (green) / blocked (red) cell outlines at floorY (F3 / debug overlay). */
	void AppendDebugDraw(FDebugDraw& Draw) const;

private:
	void BakeGrid(const FPhysScene& Physics, float FloorY, float WalkBounds, const ULevel* Level);

	FNavMesh Mesh{};
	/** Smallest cell size / agent radius (cm). */
	static constexpr float MinBakeSize = 5.0f;
	/** cm */
	float CellSize = 50.0f;
	/** cm */
	float AgentRadius = 35.0f;
	int BlockerCount = 0;
	int WalkableCellCount = 0;
};
