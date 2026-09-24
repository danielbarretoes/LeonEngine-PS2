#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include "Debug/DebugDraw.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include <numbers>
#include <vector>

namespace {

constexpr glm::vec3 kTraceMiss{0.25f, 0.85f, 1.0f};
constexpr glm::vec3 kTraceHitPath{0.2f, 1.0f, 0.35f};
constexpr glm::vec3 kTraceBeyond{1.0f, 0.25f, 0.2f};
constexpr glm::vec3 kTraceNormal{1.0f, 0.9f, 0.2f};
constexpr glm::vec3 kTraceShape{0.95f, 0.45f, 1.0f};

[[nodiscard]] bool bodyMatchesChannel(const FBodyInstance& body, ECollisionChannel channel) {
    switch (channel) {
    case ECollisionChannel::WorldStatic:
        return body.type == EBodyType::Static;
    case ECollisionChannel::WorldDynamic:
        return body.type == EBodyType::Dynamic;
    case ECollisionChannel::Pawn:
    case ECollisionChannel::Visibility:
        return true;
    }
    return true;
}

[[nodiscard]] bool segmentAabb(const glm::vec3& start, const glm::vec3& end, const glm::vec3& mn,
                               const glm::vec3& mx, float& outT, glm::vec3& outNormal) {
    const glm::vec3 dir = end - start;
    float tEnter = 0.0f;
    float tExit = 1.0f;
    glm::vec3 enterNormal{0.0f, 1.0f, 0.0f};
    bool hitFace = false;

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(dir[axis]) < 1.0e-8f) {
            if (start[axis] < mn[axis] || start[axis] > mx[axis]) {
                return false;
            }
            continue;
        }

        const float inv = 1.0f / dir[axis];
        float t0 = (mn[axis] - start[axis]) * inv;
        float t1 = (mx[axis] - start[axis]) * inv;
        float normalSign = -1.0f;
        if (inv < 0.0f) {
            std::swap(t0, t1);
            normalSign = 1.0f;
        }

        if (t0 > tEnter) {
            tEnter = t0;
            enterNormal = {};
            enterNormal[axis] = normalSign;
            hitFace = true;
        }
        tExit = std::min(tExit, t1);
        if (tEnter > tExit) {
            return false;
        }
    }

    if (tEnter < 0.0f || tEnter > 1.0f) {
        return false;
    }

    if (!hitFace && tEnter <= 0.0f) {
        outT = 0.0f;
        outNormal = {0.0f, 1.0f, 0.0f};
        return true;
    }

    outT = tEnter;
    outNormal = enterNormal;
    const float len = glm::length(outNormal);
    if (len > 1.0e-6f) {
        outNormal /= len;
    }
    return true;
}

[[nodiscard]] bool segmentFloorY(const glm::vec3& start, const glm::vec3& end, float floorY,
                                 float& outT, glm::vec3& outNormal) {
    const float dy = end.y - start.y;
    if (std::abs(dy) < 1.0e-8f) {
        return false;
    }
    const float t = (floorY - start.y) / dy;
    if (t < 0.0f || t > 1.0f) {
        return false;
    }
    outT = t;
    outNormal = {0.0f, dy < 0.0f ? 1.0f : -1.0f, 0.0f};
    return true;
}

[[nodiscard]] bool pointInAabb(const glm::vec3& p, const glm::vec3& center,
                               const glm::vec3& halfExtents, float inflate) {
    const glm::vec3 he = halfExtents + glm::vec3{inflate};
    return std::abs(p.x - center.x) <= he.x && std::abs(p.y - center.y) <= he.y &&
           std::abs(p.z - center.z) <= he.z;
}

/// Segment vs FSlopePlane. `inflate` expands the plane along its normal (sphere/capsule radius).
/// Hits when the offset-plane distance changes sign (approach from either side).
[[nodiscard]] bool segmentSlopePlane(const glm::vec3& start, const glm::vec3& end,
                                     const FSlopePlane& plane, float inflate, float& outT,
                                     glm::vec3& outNormal) {
    const float nLen = glm::length(plane.normal);
    if (nLen < 1.0e-6f) {
        return false;
    }
    const glm::vec3 n = plane.normal / nLen;
    const float d0 = glm::dot(start - plane.point, n) - inflate;
    const float d1 = glm::dot(end - plane.point, n) - inflate;
    if (d0 < 0.0f && d1 < 0.0f) {
        return false;
    }
    if (d0 > 0.0f && d1 > 0.0f) {
        return false;
    }
    const float denom = d0 - d1;
    if (std::abs(denom) < 1.0e-8f) {
        return false;
    }
    const float t = d0 / denom;
    if (t < 0.0f || t > 1.0f) {
        return false;
    }
    const glm::vec3 hitLoc = start + ((end - start) * t);
    if (!pointInAabb(hitLoc, plane.boundsCenter, plane.boundsHalfExtents, inflate)) {
        return false;
    }
    outT = t;
    outNormal = (d0 >= 0.0f) ? n : -n;
    return true;
}

void writeHit(FHitResult& out, const glm::vec3& start, const glm::vec3& end, float t,
              const glm::vec3& normal, std::size_t levelMeshIndex, bool floorPlane) {
    const glm::vec3 delta = end - start;
    const float segLen = glm::length(delta);
    out.bBlockingHit = true;
    out.Time = t;
    out.Distance = segLen * t;
    out.Location = start + (delta * t);
    out.ImpactPoint = out.Location;
    out.ImpactNormal = normal;
    out.TraceStart = start;
    out.TraceEnd = end;
    out.LevelMeshIndex = levelMeshIndex;
    out.bFloorPlane = floorPlane;
}

void appendSlopePlaneHits(std::vector<FHitResult>& outHits, const glm::vec3& start,
                          const glm::vec3& end, float radius, float halfHeight,
                          const std::vector<FSlopePlane>& planes) {
    for (const FSlopePlane& plane : planes) {
        const float nLen = glm::length(plane.normal);
        const float ny = (nLen > 1.0e-6f) ? (std::abs(plane.normal.y) / nLen) : 1.0f;
        const float inflate = radius + (halfHeight * ny);
        float t = 1.0f;
        glm::vec3 normal{};
        if (!segmentSlopePlane(start, end, plane, inflate, t, normal)) {
            continue;
        }
        FHitResult hit{};
        writeHit(hit, start, end, t, normal, Level::npos, false);
        hit.ImpactPoint = hit.Location - (normal * inflate);
        outHits.push_back(hit);
    }
}

void sortHitsByTime(std::vector<FHitResult>& hits) {
    std::sort(hits.begin(), hits.end(),
              [](const FHitResult& a, const FHitResult& b) { return a.Time < b.Time; });
}

/// Copy nearest Multi hit into `outHit` (Unreal Single returns the first blocking hit).
[[nodiscard]] bool takeNearestHit(const std::vector<FHitResult>& hits, FHitResult& outHit,
                                  const glm::vec3& start, const glm::vec3& end) {
    outHit = {};
    outHit.TraceStart = start;
    outHit.TraceEnd = end;
    outHit.Time = 1.0f;
    if (hits.empty()) {
        return false;
    }
    outHit = hits.front();
    return true;
}

void addRingXZ(FDebugDraw& draw, const glm::vec3& center, float radius, const glm::vec3& color,
               int segments = 16) {
    const float segCount = static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segCount) * (2.0f * std::numbers::pi_v<float>);
        const float a1 =
            (static_cast<float>(i + 1) / segCount) * (2.0f * std::numbers::pi_v<float>);
        draw.AddLine(center + glm::vec3{std::cos(a0) * radius, 0.0f, std::sin(a0) * radius},
                     center + glm::vec3{std::cos(a1) * radius, 0.0f, std::sin(a1) * radius}, color);
    }
}

void addImpactMarker(FDebugDraw& draw, const FHitResult& hit) {
    const float s = 0.08f;
    draw.AddLine(hit.ImpactPoint + glm::vec3{-s, 0, 0}, hit.ImpactPoint + glm::vec3{s, 0, 0},
                 kTraceNormal);
    draw.AddLine(hit.ImpactPoint + glm::vec3{0, -s, 0}, hit.ImpactPoint + glm::vec3{0, s, 0},
                 kTraceNormal);
    draw.AddLine(hit.ImpactPoint + glm::vec3{0, 0, -s}, hit.ImpactPoint + glm::vec3{0, 0, s},
                 kTraceNormal);
    draw.AddArrow(hit.ImpactPoint, hit.ImpactPoint + (hit.ImpactNormal * 0.45f), kTraceNormal,
                  0.12f, 0.07f);
}

void drawTracePath(FDebugDraw& draw, const glm::vec3& start, const glm::vec3& end,
                   const std::vector<FHitResult>& hits) {
    if (hits.empty()) {
        draw.AddLine(start, end, kTraceMiss);
        return;
    }
    const FHitResult& first = hits.front();
    draw.AddLine(start, first.ImpactPoint, kTraceHitPath);
    draw.AddLine(first.ImpactPoint, end, kTraceBeyond);
    for (const FHitResult& hit : hits) {
        addImpactMarker(draw, hit);
    }
}

[[nodiscard]] bool shouldDraw(const FCollisionQueryParams& params, FDebugDraw* debugDraw) {
    return debugDraw != nullptr && params.DrawDebugType == EDrawDebugTrace::ForOneFrame;
}

} // namespace

void DrawDebugLineTrace(FDebugDraw& draw, const glm::vec3& start, const glm::vec3& end,
                        const std::vector<FHitResult>& hits) {
    drawTracePath(draw, start, end, hits);
}

void DrawDebugSphereTrace(FDebugDraw& draw, const glm::vec3& start, const glm::vec3& end,
                          float radius, const std::vector<FHitResult>& hits) {
    const float r = std::max(radius, 0.0f);
    drawTracePath(draw, start, end, hits);
    addRingXZ(draw, start, r, kTraceShape);
    addRingXZ(draw, end, r, kTraceShape);
    if (!hits.empty()) {
        addRingXZ(draw, hits.front().ImpactPoint + glm::vec3{0.0f, r, 0.0f}, r, kTraceHitPath);
    }
}

void DrawDebugCapsuleTrace(FDebugDraw& draw, const glm::vec3& start, const glm::vec3& end,
                           float radius, float halfHeight, const std::vector<FHitResult>& hits) {
    const float r = std::max(radius, 0.0f);
    const float hh = std::max(halfHeight, 0.0f);
    drawTracePath(draw, start, end, hits);
    addRingXZ(draw, start + glm::vec3{0.0f, hh, 0.0f}, r, kTraceShape);
    addRingXZ(draw, start - glm::vec3{0.0f, hh, 0.0f}, r, kTraceShape);
    addRingXZ(draw, end + glm::vec3{0.0f, hh, 0.0f}, r, kTraceShape);
    addRingXZ(draw, end - glm::vec3{0.0f, hh, 0.0f}, r, kTraceShape);
    draw.AddLine(start + glm::vec3{r, -hh, 0.0f}, start + glm::vec3{r, hh, 0.0f}, kTraceShape);
    draw.AddLine(start + glm::vec3{-r, -hh, 0.0f}, start + glm::vec3{-r, hh, 0.0f}, kTraceShape);
    if (!hits.empty()) {
        addRingXZ(draw, hits.front().ImpactPoint, r, kTraceHitPath);
    }
}

bool FPhysScene::LineTraceMultiByChannel(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                        const glm::vec3& end, ECollisionChannel channel,
                                        const FCollisionQueryParams& params,
                                        FDebugDraw* debugDraw) const {
    outHits.clear();

    if (backendIface_ != nullptr && backendIface_->HasNarrowPhaseTraces()) {
        // BodyInstances may have been nudged (CMC) without a Step — sync before CastRay.
        if (backendIface_->HasRigidWorld()) {
            backendIface_->RigidPrepareStep(bodies_, params.SkipLevelMeshIndex);
        }
        (void)backendIface_->RigidLineTrace(outHits, start, end, channel,
                                            params.SkipLevelMeshIndex);
    } else {
        for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
            const FBodyInstance& body = bodies_[bi];
            if (body.levelMeshIndex == params.SkipLevelMeshIndex) {
                continue;
            }
            if (!bodyMatchesChannel(body, channel)) {
                continue;
            }
            const glm::vec3 mn = body.position - body.halfExtents;
            const glm::vec3 mx = body.position + body.halfExtents;
            float tAabb = 1.0f;
            glm::vec3 nAabb{};
            if (!segmentAabb(start, end, mn, mx, tAabb, nAabb)) {
                continue;
            }

            float t = tAabb;
            glm::vec3 normal = nAabb;
            if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triangleMeshes_.size() &&
                triangleMeshes_[bi].IsValid()) {
                float tMesh = 1.0f;
                glm::vec3 nMesh{};
                if (!SegmentTriangleMesh(start, end, triangleMeshes_[bi], 0.0f, tMesh, nMesh)) {
                    continue; // Broadphase only — no actual triangle hit.
                }
                t = tMesh;
                normal = nMesh;
            }

            FHitResult hit{};
            writeHit(hit, start, end, t, normal, body.levelMeshIndex, false);
            outHits.push_back(hit);
        }
    }

    if (params.bTraceFloorPlane) {
        float t = 1.0f;
        glm::vec3 normal{};
        if (segmentFloorY(start, end, params.FloorY, t, normal)) {
            FHitResult hit{};
            writeHit(hit, start, end, t, normal, Level::npos, true);
            outHits.push_back(hit);
        }
    }
    appendSlopePlaneHits(outHits, start, end, 0.0f, 0.0f, slopePlanes_);

    sortHitsByTime(outHits);
    if (shouldDraw(params, debugDraw)) {
        DrawDebugLineTrace(*debugDraw, start, end, outHits);
    }
    return !outHits.empty();
}

bool FPhysScene::LineTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                         const glm::vec3& end, ECollisionChannel channel,
                                         const FCollisionQueryParams& params,
                                         FDebugDraw* debugDraw) const {
    std::vector<FHitResult> hits;
    (void)LineTraceMultiByChannel(hits, start, end, channel, params, debugDraw);
    return takeNearestHit(hits, outHit, start, end);
}

bool FPhysScene::SphereTraceMultiByChannel(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                          const glm::vec3& end, float radius,
                                          ECollisionChannel channel,
                                          const FCollisionQueryParams& params,
                                          FDebugDraw* debugDraw) const {
    const float r = std::max(radius, 0.0f);
    outHits.clear();

    if (backendIface_ != nullptr && backendIface_->HasNarrowPhaseTraces()) {
        // Push FBodyInstance → Jolt before CastShape (same as Step prepare).
        if (backendIface_->HasRigidWorld()) {
            backendIface_->RigidPrepareStep(bodies_, params.SkipLevelMeshIndex);
        }
        (void)backendIface_->RigidSphereTrace(outHits, start, end, r, channel,
                                              params.SkipLevelMeshIndex);
    } else {
        for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
            const FBodyInstance& body = bodies_[bi];
            if (body.levelMeshIndex == params.SkipLevelMeshIndex) {
                continue;
            }
            if (!bodyMatchesChannel(body, channel)) {
                continue;
            }
            const glm::vec3 expand{r, r, r};
            const glm::vec3 mn = body.position - body.halfExtents - expand;
            const glm::vec3 mx = body.position + body.halfExtents + expand;
            float tAabb = 1.0f;
            glm::vec3 nAabb{};
            if (!segmentAabb(start, end, mn, mx, tAabb, nAabb)) {
                continue;
            }

            float t = tAabb;
            glm::vec3 normal = nAabb;
            if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triangleMeshes_.size() &&
                triangleMeshes_[bi].IsValid()) {
                float tMesh = 1.0f;
                glm::vec3 nMesh{};
                if (!SegmentTriangleMesh(start, end, triangleMeshes_[bi], r, tMesh, nMesh)) {
                    continue;
                }
                t = tMesh;
                normal = nMesh;
            }

            FHitResult hit{};
            writeHit(hit, start, end, t, normal, body.levelMeshIndex, false);
            // Unreal FHitResult: Location = sweep shape center; ImpactPoint = surface contact.
            hit.ImpactPoint = hit.Location - (normal * r);
            outHits.push_back(hit);
        }
    }

    if (params.bTraceFloorPlane) {
        const float planeY = params.FloorY + r;
        float t = 1.0f;
        glm::vec3 normal{};
        if (segmentFloorY(start, end, planeY, t, normal)) {
            FHitResult hit{};
            writeHit(hit, start, end, t, normal, Level::npos, true);
            hit.ImpactPoint = hit.Location - (normal * r);
            outHits.push_back(hit);
        }
    }
    appendSlopePlaneHits(outHits, start, end, r, 0.0f, slopePlanes_);

    sortHitsByTime(outHits);
    if (shouldDraw(params, debugDraw)) {
        DrawDebugSphereTrace(*debugDraw, start, end, r, outHits);
    }
    return !outHits.empty();
}

bool FPhysScene::SphereTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                           const glm::vec3& end, float radius,
                                           ECollisionChannel channel,
                                           const FCollisionQueryParams& params,
                                           FDebugDraw* debugDraw) const {
    std::vector<FHitResult> hits;
    (void)SphereTraceMultiByChannel(hits, start, end, radius, channel, params, debugDraw);
    return takeNearestHit(hits, outHit, start, end);
}

bool FPhysScene::CapsuleTraceMultiByChannel(std::vector<FHitResult>& outHits, const glm::vec3& start,
                                           const glm::vec3& end, float radius, float halfHeight,
                                           ECollisionChannel channel,
                                           const FCollisionQueryParams& params,
                                           FDebugDraw* debugDraw) const {
    const float r = std::max(radius, 0.0f);
    const float hh = std::max(halfHeight, 0.0f);
    outHits.clear();
    const glm::vec3 expand{r, hh + r, r};

    if (backendIface_ != nullptr && backendIface_->HasNarrowPhaseTraces()) {
        if (backendIface_->HasRigidWorld()) {
            backendIface_->RigidPrepareStep(bodies_, params.SkipLevelMeshIndex);
        }
        (void)backendIface_->RigidCapsuleTrace(outHits, start, end, r, hh, channel,
                                               params.SkipLevelMeshIndex);
    } else {
        for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
            const FBodyInstance& body = bodies_[bi];
            if (body.levelMeshIndex == params.SkipLevelMeshIndex) {
                continue;
            }
            if (!bodyMatchesChannel(body, channel)) {
                continue;
            }
            const glm::vec3 mn = body.position - body.halfExtents - expand;
            const glm::vec3 mx = body.position + body.halfExtents + expand;
            float tAabb = 1.0f;
            glm::vec3 nAabb{};
            if (!segmentAabb(start, end, mn, mx, tAabb, nAabb)) {
                continue;
            }

            float t = tAabb;
            glm::vec3 normal = nAabb;
            if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triangleMeshes_.size() &&
                triangleMeshes_[bi].IsValid()) {
                // Inflate like slope planes: radius + |ny|*halfHeight approximated via radius+hh.
                float tMesh = 1.0f;
                glm::vec3 nMesh{};
                if (!SegmentTriangleMesh(start, end, triangleMeshes_[bi], r + hh, tMesh, nMesh)) {
                    continue;
                }
                t = tMesh;
                normal = nMesh;
            }

            FHitResult hit{};
            writeHit(hit, start, end, t, normal, body.levelMeshIndex, false);
            const float pull = (std::abs(normal.y) > 0.5f) ? (hh + r) : r;
            hit.ImpactPoint = hit.Location - (normal * pull);
            outHits.push_back(hit);
        }
    }

    if (params.bTraceFloorPlane) {
        const float planeY = params.FloorY + hh + r;
        float t = 1.0f;
        glm::vec3 normal{};
        if (segmentFloorY(start, end, planeY, t, normal)) {
            FHitResult hit{};
            writeHit(hit, start, end, t, normal, Level::npos, true);
            hit.ImpactPoint = hit.Location - (normal * (hh + r));
            outHits.push_back(hit);
        }
    }
    appendSlopePlaneHits(outHits, start, end, r, hh, slopePlanes_);

    sortHitsByTime(outHits);
    if (shouldDraw(params, debugDraw)) {
        DrawDebugCapsuleTrace(*debugDraw, start, end, r, hh, outHits);
    }
    return !outHits.empty();
}

bool FPhysScene::CapsuleTraceSingleByChannel(FHitResult& outHit, const glm::vec3& start,
                                            const glm::vec3& end, float radius, float halfHeight,
                                            ECollisionChannel channel,
                                            const FCollisionQueryParams& params,
                                            FDebugDraw* debugDraw) const {
    std::vector<FHitResult> hits;
    (void)CapsuleTraceMultiByChannel(hits, start, end, radius, halfHeight, channel, params,
                                     debugDraw);
    return takeNearestHit(hits, outHit, start, end);
}

