#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Debug/DebugDraw.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Level.h"
#include "BodyInstance.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include <limits>
#include <queue>
#include <vector>

namespace {

[[nodiscard]] bool IsFloorLikeBody(const BodyInstance& body, float cellSize) {
    const float hy = std::max(body.halfExtents.y, 0.001f);
    const float horiz = std::max(body.halfExtents.x, body.halfExtents.z);
    // Unit Plane scaled ~40x1x40 → hy=0.5 still floor-like by aspect (was wrongly a full-arena
    // blocker).
    if (horiz / hy >= 6.0f) {
        return true;
    }
    if (body.halfExtents.y <= std::max(0.35f, cellSize * 0.75f)) {
        return true;
    }
    return false;
}

[[nodiscard]] bool IsForcedNavBlockerTag(const Level& level, std::size_t meshIndex) {
    if (meshIndex >= level.StaticMeshes().size()) {
        return false;
    }
    // Thin pads / volumes: keep as obstacle so paths go around (not climbable floor).
    return level.StaticMeshes()[meshIndex].tag == NavTags::Blocker;
}

/// Walkable for CMC (slopes) — must not carve a hole in the flat grid NavMesh.
[[nodiscard]] bool IsWalkableNavSurfaceTag(const Level& level, std::size_t meshIndex) {
    if (meshIndex >= level.StaticMeshes().size()) {
        return false;
    }
    return level.StaticMeshes()[meshIndex].tag == NavTags::Walkable;
}

[[nodiscard]] bool ShouldSkipLevelMesh(const Level& level, std::size_t meshIndex) {
    if (meshIndex >= level.StaticMeshes().size()) {
        return false;
    }
    // Arena floor Plane only.
    return level.StaticMeshes()[meshIndex].editorClass == "Plane";
}

[[nodiscard]] bool BodyBlocksNavigation(const BodyInstance& body, float floorY, float cellSize,
                                        const Level* level) {
    if (body.type != EBodyType::Static) {
        return false;
    }
    if (level != nullptr && ShouldSkipLevelMesh(*level, body.levelMeshIndex)) {
        return false;
    }
    // NavWalkable (ramps): path across footprint; CharacterMovement climbs the mesh.
    if (level != nullptr && IsWalkableNavSurfaceTag(*level, body.levelMeshIndex)) {
        return false;
    }
    const float bottom = body.position.y - body.halfExtents.y;
    const float top = body.position.y + body.halfExtents.y;
    const bool inHeightBand = top > floorY + 0.05f && bottom < floorY + 2.2f;
    if (!inHeightBand) {
        return false;
    }
    // NavBlocker: thin slab may look floor-like by aspect but must block paths.
    if (level != nullptr && IsForcedNavBlockerTag(*level, body.levelMeshIndex)) {
        return true;
    }
    if (IsFloorLikeBody(body, cellSize)) {
        return false;
    }
    return true;
}

[[nodiscard]] bool AabbXZOverlapsPoint(float cx, float cz, float inflate, float minX, float maxX,
                                       float minZ, float maxZ) {
    return cx >= (minX - inflate) && cx <= (maxX + inflate) && cz >= (minZ - inflate) &&
           cz <= (maxZ + inflate);
}

[[nodiscard]] bool CellBlockedByBody(float cx, float cz, float cellHalf, float agentRadius,
                                     const BodyInstance& body) {
    const float inflate = agentRadius + cellHalf;
    return AabbXZOverlapsPoint(
        cx, cz, inflate, body.position.x - body.halfExtents.x, body.position.x + body.halfExtents.x,
        body.position.z - body.halfExtents.z, body.position.z + body.halfExtents.z);
}

/// Tighter XZ footprint from baked tris (rotated ramp) vs fat world AABB.
[[nodiscard]] bool CellBlockedByTriangleMesh(float cx, float cz, float cellHalf, float agentRadius,
                                             const TriangleMeshCollision& mesh) {
    const float inflate = agentRadius + cellHalf;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const glm::vec3& v0 = mesh.positions[mesh.indices[i]];
        const glm::vec3& v1 = mesh.positions[mesh.indices[i + 1]];
        const glm::vec3& v2 = mesh.positions[mesh.indices[i + 2]];
        const float minX = std::min({v0.x, v1.x, v2.x});
        const float maxX = std::max({v0.x, v1.x, v2.x});
        const float minZ = std::min({v0.z, v1.z, v2.z});
        const float maxZ = std::max({v0.z, v1.z, v2.z});
        if (AabbXZOverlapsPoint(cx, cz, inflate, minX, maxX, minZ, maxZ)) {
            return true;
        }
    }
    return false;
}

struct AStarNode {
    int ix = 0;
    int iz = 0;
    float f = 0.0f;
};

struct AStarNodeGreater {
    bool operator()(const AStarNode& a, const AStarNode& b) const { return a.f > b.f; }
};

[[nodiscard]] float Heuristic(int ax, int az, int bx, int bz) {
    const float dx = static_cast<float>(ax - bx);
    const float dz = static_cast<float>(az - bz);
    return std::sqrt(dx * dx + dz * dz);
}

[[nodiscard]] int CellIndex(int ix, int iz, int width) {
    return iz * width + ix;
}

} // namespace

void NavigationSystem::Clear() {
    mesh_ = {};
    blockerCount_ = 0;
    walkableCellCount_ = 0;
}

void NavigationSystem::BakeGrid(const PhysScene& physics, float floorY, float walkBounds,
                                const Level* level) {
    Clear();
    const float bounds = walkBounds > 1.0f ? walkBounds : 1.0f;
    const float cell = cellSize_;
    const int dim = std::max(4, static_cast<int>(std::ceil((bounds * 2.0f) / cell)));

    mesh_.originX = -bounds;
    mesh_.originZ = -bounds;
    mesh_.cellSize = cell;
    mesh_.floorY = floorY;
    mesh_.width = dim;
    mesh_.depth = dim;
    mesh_.walkable.assign(static_cast<std::size_t>(dim * dim), 1);

    const float cellHalf = cell * 0.5f;
    struct NavBlocker {
        const BodyInstance* body = nullptr;
        const TriangleMeshCollision* triMesh = nullptr;
    };
    std::vector<NavBlocker> blockers;
    blockers.reserve(physics.Bodies().size());
    const auto& triMeshes = physics.TriangleMeshes();
    for (std::size_t bi = 0; bi < physics.Bodies().size(); ++bi) {
        const BodyInstance& body = physics.Bodies()[bi];
        if (!BodyBlocksNavigation(body, floorY, cell, level)) {
            continue;
        }
        NavBlocker blocker{};
        blocker.body = &body;
        if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triMeshes.size() &&
            triMeshes[bi].IsValid()) {
            blocker.triMesh = &triMeshes[bi];
        }
        blockers.push_back(blocker);
    }
    blockerCount_ = static_cast<int>(blockers.size());

    int walkable = 0;
    for (int iz = 0; iz < dim; ++iz) {
        for (int ix = 0; ix < dim; ++ix) {
            const glm::vec3 center = mesh_.CellCenter(ix, iz);
            bool blocked = false;
            for (const NavBlocker& blocker : blockers) {
                if (blocker.triMesh != nullptr) {
                    if (CellBlockedByTriangleMesh(center.x, center.z, cellHalf, agentRadius_,
                                                  *blocker.triMesh)) {
                        blocked = true;
                        break;
                    }
                } else if (CellBlockedByBody(center.x, center.z, cellHalf, agentRadius_,
                                             *blocker.body)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) {
                mesh_.walkable[static_cast<std::size_t>(CellIndex(ix, iz, dim))] = 0;
            } else {
                ++walkable;
            }
        }
    }

    // Extra clearance dilation beyond per-sample inflate (agents larger than one cell).
    const int dilateRings = std::max(0, static_cast<int>(std::ceil(agentRadius_ / cell)) - 1);
    if (dilateRings > 0) {
        std::vector<std::uint8_t> dilated = mesh_.walkable;
        for (int iz = 0; iz < dim; ++iz) {
            for (int ix = 0; ix < dim; ++ix) {
                if (mesh_.walkable[static_cast<std::size_t>(CellIndex(ix, iz, dim))] == 0) {
                    continue;
                }
                bool nearBlocked = false;
                for (int dz = -dilateRings; dz <= dilateRings && !nearBlocked; ++dz) {
                    for (int dx = -dilateRings; dx <= dilateRings; ++dx) {
                        const int nx = ix + dx;
                        const int nz = iz + dz;
                        if (nx < 0 || nz < 0 || nx >= dim || nz >= dim) {
                            continue;
                        }
                        if (mesh_.walkable[static_cast<std::size_t>(CellIndex(nx, nz, dim))] == 0) {
                            nearBlocked = true;
                            break;
                        }
                    }
                }
                if (nearBlocked) {
                    dilated[static_cast<std::size_t>(CellIndex(ix, iz, dim))] = 0;
                }
            }
        }
        mesh_.walkable.swap(dilated);
        walkable = 0;
        for (std::uint8_t w : mesh_.walkable) {
            walkable += w != 0 ? 1 : 0;
        }
    }
    walkableCellCount_ = walkable;
}

void NavigationSystem::BuildFromPhysScene(const PhysScene& physics, float floorY,
                                          float walkBounds) {
    BakeGrid(physics, floorY, walkBounds, nullptr);
}

void NavigationSystem::BuildFromLevel(const Level& level, const PhysScene& physics, float floorY,
                                      float walkBounds) {
    BakeGrid(physics, floorY, walkBounds, &level);
}

bool NavigationSystem::ProjectPointToNavigation(const glm::vec3& world,
                                                glm::vec3& outProjected) const {
    if (!mesh_.IsValid()) {
        return false;
    }
    int ix = 0;
    int iz = 0;
    if (!mesh_.WorldToCell(world.x, world.z, ix, iz)) {
        return false;
    }
    if (mesh_.IsWalkable(ix, iz)) {
        outProjected = mesh_.CellCenter(ix, iz);
        return true;
    }
    // Spiral search for nearest walkable cell.
    const int maxR = std::max(mesh_.width, mesh_.depth);
    for (int r = 1; r <= maxR; ++r) {
        for (int dz = -r; dz <= r; ++dz) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dz) != r) {
                    continue;
                }
                const int nx = ix + dx;
                const int nz = iz + dz;
                if (mesh_.IsWalkable(nx, nz)) {
                    outProjected = mesh_.CellCenter(nx, nz);
                    return true;
                }
            }
        }
    }
    return false;
}

bool NavigationSystem::FindPath(const glm::vec3& start, const glm::vec3& end,
                                std::vector<glm::vec3>& outPath) const {
    outPath.clear();
    if (!mesh_.IsValid() || walkableCellCount_ <= 0) {
        return false;
    }

    glm::vec3 startNav{};
    glm::vec3 endNav{};
    if (!ProjectPointToNavigation(start, startNav) || !ProjectPointToNavigation(end, endNav)) {
        return false;
    }

    int sx = 0;
    int sz = 0;
    int ex = 0;
    int ez = 0;
    if (!mesh_.WorldToCell(startNav.x, startNav.z, sx, sz) ||
        !mesh_.WorldToCell(endNav.x, endNav.z, ex, ez)) {
        return false;
    }
    if (!mesh_.IsWalkable(sx, sz) || !mesh_.IsWalkable(ex, ez)) {
        return false;
    }
    if (sx == ex && sz == ez) {
        outPath.push_back(endNav);
        return true;
    }

    const int width = mesh_.width;
    const int depth = mesh_.depth;
    const int cellCount = width * depth;
    std::vector<float> gScore(static_cast<std::size_t>(cellCount),
                              std::numeric_limits<float>::infinity());
    std::vector<int> cameFrom(static_cast<std::size_t>(cellCount), -1);
    std::vector<std::uint8_t> closed(static_cast<std::size_t>(cellCount), 0);

    std::priority_queue<AStarNode, std::vector<AStarNode>, AStarNodeGreater> open;
    const int startIdx = CellIndex(sx, sz, width);
    gScore[static_cast<std::size_t>(startIdx)] = 0.0f;
    open.push(AStarNode{sx, sz, Heuristic(sx, sz, ex, ez)});

    static constexpr int kDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int kDz[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static constexpr float kCost[8] = {1.4142f, 1.0f, 1.4142f, 1.0f, 1.0f, 1.4142f, 1.0f, 1.4142f};

    bool found = false;
    while (!open.empty()) {
        const AStarNode cur = open.top();
        open.pop();
        const int curIdx = CellIndex(cur.ix, cur.iz, width);
        if (closed[static_cast<std::size_t>(curIdx)] != 0) {
            continue;
        }
        closed[static_cast<std::size_t>(curIdx)] = 1;
        if (cur.ix == ex && cur.iz == ez) {
            found = true;
            break;
        }

        for (int i = 0; i < 8; ++i) {
            const int nx = cur.ix + kDx[i];
            const int nz = cur.iz + kDz[i];
            if (!mesh_.IsWalkable(nx, nz)) {
                continue;
            }
            // No corner-cutting through blocked diagonals.
            if (kDx[i] != 0 && kDz[i] != 0) {
                if (!mesh_.IsWalkable(cur.ix + kDx[i], cur.iz) ||
                    !mesh_.IsWalkable(cur.ix, cur.iz + kDz[i])) {
                    continue;
                }
            }
            const int nIdx = CellIndex(nx, nz, width);
            if (closed[static_cast<std::size_t>(nIdx)] != 0) {
                continue;
            }
            const float tentative = gScore[static_cast<std::size_t>(curIdx)] + kCost[i];
            if (tentative >= gScore[static_cast<std::size_t>(nIdx)]) {
                continue;
            }
            cameFrom[static_cast<std::size_t>(nIdx)] = curIdx;
            gScore[static_cast<std::size_t>(nIdx)] = tentative;
            open.push(AStarNode{nx, nz, tentative + Heuristic(nx, nz, ex, ez)});
        }
    }

    if (!found) {
        return false;
    }

    std::vector<glm::vec3> reverse;
    int idx = CellIndex(ex, ez, width);
    while (idx >= 0) {
        const int ix = idx % width;
        const int iz = idx / width;
        reverse.push_back(mesh_.CellCenter(ix, iz));
        idx = cameFrom[static_cast<std::size_t>(idx)];
    }
    std::reverse(reverse.begin(), reverse.end());
    if (!reverse.empty()) {
        reverse.back() = endNav;
    }
    outPath = std::move(reverse);
    return !outPath.empty();
}

void NavigationSystem::AppendDebugDraw(DebugDraw& draw) const {
    if (!mesh_.IsValid()) {
        return;
    }

    const float y = mesh_.floorY + 0.04f;
    const float half = mesh_.cellSize * 0.5f;
    constexpr glm::vec3 kWalkable{0.15f, 0.85f, 0.35f};
    constexpr glm::vec3 kBlocked{0.95f, 0.2f, 0.15f};

    for (int iz = 0; iz < mesh_.depth; ++iz) {
        for (int ix = 0; ix < mesh_.width; ++ix) {
            const glm::vec3 center = mesh_.CellCenter(ix, iz);
            const float x0 = center.x - half;
            const float x1 = center.x + half;
            const float z0 = center.z - half;
            const float z1 = center.z + half;
            const glm::vec3& color = mesh_.IsWalkable(ix, iz) ? kWalkable : kBlocked;
            draw.AddLine({x0, y, z0}, {x1, y, z0}, color);
            draw.AddLine({x1, y, z0}, {x1, y, z1}, color);
            draw.AddLine({x1, y, z1}, {x0, y, z1}, color);
            draw.AddLine({x0, y, z1}, {x0, y, z0}, color);
            if (!mesh_.IsWalkable(ix, iz)) {
                draw.AddLine({x0, y, z0}, {x1, y, z1}, color);
                draw.AddLine({x1, y, z0}, {x0, y, z1}, color);
            }
        }
    }
}

