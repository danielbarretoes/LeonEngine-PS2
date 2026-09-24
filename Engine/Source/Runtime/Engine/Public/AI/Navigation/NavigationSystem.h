#pragma once

#include "AI/Navigation/NavMesh.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <vector>

class ULevel;
class FPhysScene;
class FDebugDraw;

/// Level mesh tags recognized by UNavigationSystem bake (Unreal NavArea-style hints).
namespace NavTags
{
	inline constexpr const char* Blocker = "NavBlocker";
	inline constexpr const char* Walkable = "NavWalkable";
} // namespace NavTags

/// Unreal-like UNavigationSystem lite: bake a grid NavMesh from static FPhysScene bodies,
/// then FindPath for AAIController. Not Recast/Detour — swap-compatible later.
class ENGINE_API UNavigationSystem
{
public:
	/// Cell size / agent radius used when baking (defaults ~ character capsule radius).
	void SetCellSize(float Meters)
	{
		CellSize = Meters > 0.05f ? Meters : 0.05f;
	}
	void SetAgentRadius(float Meters)
	{
		AgentRadius = Meters > 0.05f ? Meters : 0.05f;
	}
	[[nodiscard]] float GetCellSize() const
	{
		return CellSize;
	}
	[[nodiscard]] float GetAgentRadius() const
	{
		return AgentRadius;
	}

	/// Bake walkable grid from static box bodies. Wide/flat floor slabs stay walkable.
	/// `walkBounds` is half-extent from origin on XZ (matches UCharacterMovementComponent::WalkBounds).
	void BuildFromPhysScene(const FPhysScene& Physics, float FloorY, float WalkBounds);

	/// Prefer this: skips FPlane; honors NavTags::Blocker / NavTags::Walkable on meshes.
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

	/// Snap to nearest walkable cell center (Y = floorY). Returns false if no nav mesh.
	[[nodiscard]] bool ProjectPointToNavigation(const glm::vec3& World, glm::vec3& OutProjected) const;

	/// A* on the grid (8-connected). Fills `outPath` with world waypoints (includes end).
	/// Returns false if no path; `outPath` cleared on failure.
	[[nodiscard]] bool FindPath(const glm::vec3& Start, const glm::vec3& End, std::vector<glm::vec3>& OutPath) const;

	/// Draw walkable (green) / blocked (red) cell outlines at floorY (F3 / debug overlay).
	void AppendDebugDraw(FDebugDraw& Draw) const;

private:
	void BakeGrid(const FPhysScene& Physics, float FloorY, float WalkBounds, const ULevel* Level);

	FNavMesh Mesh{};
	float CellSize = 0.5f;
	float AgentRadius = 0.35f;
	int BlockerCount = 0;
	int WalkableCellCount = 0;
};
