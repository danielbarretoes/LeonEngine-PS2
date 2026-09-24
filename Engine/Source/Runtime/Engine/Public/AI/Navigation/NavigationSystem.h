#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include "AI/Navigation/NavMesh.h"
#include <vector>


class Level;
class PhysScene;
class DebugDraw;

/// Level mesh tags recognized by NavigationSystem bake (Unreal NavArea-style hints).
namespace NavTags {
inline constexpr const char* Blocker = "NavBlocker";
inline constexpr const char* Walkable = "NavWalkable";
} // namespace NavTags

/// Unreal-like NavigationSystem lite: bake a grid NavMesh from static PhysScene bodies,
/// then FindPath for AIController. Not Recast/Detour — swap-compatible later.
class NavigationSystem {
public:
    /// Cell size / agent radius used when baking (defaults ~ character capsule radius).
    void SetCellSize(float meters) { cellSize_ = meters > 0.05f ? meters : 0.05f; }
    void SetAgentRadius(float meters) { agentRadius_ = meters > 0.05f ? meters : 0.05f; }
    [[nodiscard]] float CellSize() const { return cellSize_; }
    [[nodiscard]] float AgentRadius() const { return agentRadius_; }

    /// Bake walkable grid from static box bodies. Wide/flat floor slabs stay walkable.
    /// `walkBounds` is half-extent from origin on XZ (matches CharacterMovement::WalkBounds).
    void BuildFromPhysScene(const PhysScene& physics, float floorY, float walkBounds);

    /// Prefer this: skips Plane; honors NavTags::Blocker / NavTags::Walkable on meshes.
    void BuildFromLevel(const Level& level, const PhysScene& physics, float floorY,
                        float walkBounds);

    void Clear();

    [[nodiscard]] bool HasNavMesh() const { return mesh_.IsValid(); }
    [[nodiscard]] const NavMesh& GetNavMesh() const { return mesh_; }
    [[nodiscard]] int BlockerCount() const { return blockerCount_; }
    [[nodiscard]] int WalkableCellCount() const { return walkableCellCount_; }

    /// Snap to nearest walkable cell center (Y = floorY). Returns false if no nav mesh.
    [[nodiscard]] bool ProjectPointToNavigation(const glm::vec3& world, glm::vec3& outProjected) const;

    /// A* on the grid (8-connected). Fills `outPath` with world waypoints (includes end).
    /// Returns false if no path; `outPath` cleared on failure.
    [[nodiscard]] bool FindPath(const glm::vec3& start, const glm::vec3& end,
                                std::vector<glm::vec3>& outPath) const;

    /// Draw walkable (green) / blocked (red) cell outlines at floorY (F3 / debug overlay).
    void AppendDebugDraw(DebugDraw& draw) const;

private:
    void BakeGrid(const PhysScene& physics, float floorY, float walkBounds, const Level* level);

    NavMesh mesh_{};
    float cellSize_ = 0.5f;
    float agentRadius_ = 0.35f;
    int blockerCount_ = 0;
    int walkableCellCount_ = 0;
};

