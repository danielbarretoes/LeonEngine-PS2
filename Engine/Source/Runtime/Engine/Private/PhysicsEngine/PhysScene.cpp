#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include "Debug/DebugDraw.h"
#include "IPhysicsBackend.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include "Frustum.h"
#include "MeshData.h"
#include "StaticMesh.h"

namespace leon {

PhysScene::PhysScene(EPhysicsBackend backend)
    : backend_(backend)
    , backendIface_(CreatePhysicsBackend(backend == EPhysicsBackend::Jolt ? EPhysicsBackendKind::Jolt
                                                                          : EPhysicsBackendKind::Arcade)) {
    if (backendIface_ != nullptr && std::strcmp(backendIface_->GetName(), "Jolt") == 0) {
        backend_ = EPhysicsBackend::Jolt;
    } else {
        backend_ = EPhysicsBackend::Arcade;
    }
}

namespace {

void cancelVelocityInto(glm::vec2& vel, const glm::vec2& outwardNormal) {
    const float into = glm::dot(vel, -outwardNormal);
    if (into > 0.0f) {
        vel += outwardNormal * into;
    }
}

void cancelVelocityYInto(float& velocityY, float outwardNormalY) {
    if (outwardNormalY > 0.5f && velocityY < 0.0f) {
        velocityY = 0.0f;
    } else if (outwardNormalY < -0.5f && velocityY > 0.0f) {
        velocityY = 0.0f;
    }
}

void appendCapsuleRing(DebugDraw& draw, const glm::vec3& center, float radius,
                       const glm::vec3& color, int segments) {
    const float segCount = static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segCount) * 6.2831853f;
        const float a1 = (static_cast<float>(i + 1) / segCount) * 6.2831853f;
        const glm::vec3 p0{std::cos(a0) * radius, 0.0f, std::sin(a0) * radius};
        const glm::vec3 p1{std::cos(a1) * radius, 0.0f, std::sin(a1) * radius};
        draw.AddLine(center + p0, center + p1, color);
    }
}

[[nodiscard]] float landWindow(float velocityY, float deltaTime, float skin) {
    return std::max(0.12f, (std::abs(velocityY) * deltaTime) + (skin * 4.0f));
}

/// Flow: wish into contact normal → lateral vel + optional contact shove (light props).
void applyDynamicWishPush(BodyInstance& body, const glm::vec2& wishN, const glm::vec2& normal,
                          float pushStrength, float walkBounds, bool applyContactShove) {
    const float into = std::max(0.0f, -glm::dot(wishN, normal));
    if (into <= 1.0e-4f) {
        return;
    }

    constexpr float kPlayerMass = 80.0f;
    const float bodyMass = std::max(body.mass, 0.5f);
    const float invMass = 1.0f / bodyMass;
    constexpr float kPushScale = 2.8f;
    body.velXZ += wishN * (into * pushStrength * invMass * kPushScale);

    constexpr float kMaxPushSpeed = 4.0f;
    const float speed = glm::length(body.velXZ);
    if (speed > kMaxPushSpeed) {
        body.velXZ *= kMaxPushSpeed / speed;
    }

    if (!applyContactShove) {
        return;
    }

    // Sweep-based contact never overlaps; nudge the body so walking shove is visible same frame.
    const float bodyShare = kPlayerMass / (kPlayerMass + bodyMass);
    constexpr float kContactShove = 0.06f;
    const float shove = into * pushStrength * bodyShare * kContactShove;
    body.position.x += wishN.x * shove;
    body.position.z += wishN.y * shove;
    ClampPositionXZ(body.position, walkBounds);
}

} // namespace

void PhysScene::Clear() {
    bodies_.clear();
    triangleMeshes_.clear();
    slopePlanes_.clear();
    if (backendIface_ != nullptr) {
        backendIface_->RigidClear();
    }
}

std::size_t PhysScene::AddBody(const BodyInstanceDesc& desc) {
    BodyInstance body{};
    body.levelMeshIndex = desc.levelMeshIndex;
    body.type = desc.type;
    body.mass = desc.mass > 0.0f ? desc.mass : 0.0f;
    body.enableGravity = desc.enableGravity;
    bodies_.push_back(body);
    triangleMeshes_.emplace_back();
    return bodies_.size() - 1;
}

std::size_t PhysScene::AddSlopeRamp(const glm::vec3& boundsCenter,
                                    const glm::vec3& boundsHalfExtents, float pitchDegrees,
                                    float yawDegrees) {
    constexpr float kDegToRad = 0.01745329251f;
    const float pitch = pitchDegrees * kDegToRad;
    const float s = std::sin(pitch);
    const float c = std::cos(pitch);
    SlopePlane plane{};
    plane.point = boundsCenter;
    // Surface rises with +X; unit normal points to the walkable side (normal.y = cos(pitch)).
    glm::vec3 normal{-s, c, 0.0f};
    if (std::abs(yawDegrees) > 1.0e-3f) {
        const float yaw = yawDegrees * kDegToRad;
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        normal = {normal.x * cy - normal.z * sy, normal.y, normal.x * sy + normal.z * cy};
    }
    plane.normal = glm::normalize(normal);
    plane.boundsCenter = boundsCenter;
    plane.boundsHalfExtents = boundsHalfExtents;
    slopePlanes_.push_back(plane);
    return slopePlanes_.size() - 1;
}

void PhysScene::SyncFromLevel(const Level& level) {
    const auto& meshes = level.StaticMeshes();
    if (triangleMeshes_.size() != bodies_.size()) {
        triangleMeshes_.resize(bodies_.size());
    }
    for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
        BodyInstance& body = bodies_[bi];
        TriangleMeshCollision& triMesh = triangleMeshes_[bi];
        triMesh.Clear();
        body.collisionShape = ECollisionShape::Box;

        if (body.levelMeshIndex >= meshes.size()) {
            continue;
        }
        const StaticMeshComponent& obj = meshes[body.levelMeshIndex];
        if (obj.mesh != nullptr) {
            const Aabb worldAabb = Aabb::fromLocalTransformed(
                obj.mesh->LocalMin(), obj.mesh->LocalMax(), obj.EffectiveModelMatrix());
            body.position = (worldAabb.min + worldAabb.max) * 0.5f;
            body.halfExtents = (worldAabb.max - worldAabb.min) * 0.5f;

            // Unreal ComplexAsSimple lite: static meshes with CPU tris use triangle queries.
            if (body.type == EBodyType::Static && obj.mesh->HasCpuData()) {
                const MeshData& cpu = obj.mesh->CpuData();
                const glm::mat4 model = obj.EffectiveModelMatrix();
                triMesh.positions.resize(cpu.vertices.size());
                for (std::size_t vi = 0; vi < cpu.vertices.size(); ++vi) {
                    const glm::vec4 world =
                        model * glm::vec4(cpu.vertices[vi].position, 1.0f);
                    triMesh.positions[vi] = glm::vec3(world);
                }
                triMesh.indices = cpu.indices;
                if (triMesh.IsValid()) {
                    body.collisionShape = ECollisionShape::TriangleMesh;
                } else {
                    triMesh.Clear();
                }
            }
        } else {
            body.position = obj.transform.position;
            HalfExtentsFromScale(obj.transform.scale, body.halfExtents.x, body.halfExtents.y,
                                 body.halfExtents.z);
        }
        if (body.mass <= 0.0f) {
            body.mass =
                MassFromHalfExtents(body.halfExtents.x, body.halfExtents.y, body.halfExtents.z);
        }
    }
    if (backendIface_ != nullptr && backendIface_->HasRigidWorld()) {
        backendIface_->RigidRebuild(bodies_, &triangleMeshes_);
    }
}

void PhysScene::SyncToLevel(Level& level) const {
    auto& meshes = level.StaticMeshes();
    for (const BodyInstance& body : bodies_) {
        if (body.levelMeshIndex >= meshes.size()) {
            continue;
        }
        meshes[body.levelMeshIndex].transform.position = body.position;
    }
}

float PhysScene::QuerySupportY(const CapsuleShape& capsule, const glm::vec3& feet, float floorY,
                               float stepUp, float skin, std::size_t skipLevelMeshIndex) const {
    float support = floorY;
    const float r = capsule.radius;

    for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
        const BodyInstance& body = bodies_[bi];
        if (body.levelMeshIndex == skipLevelMeshIndex) {
            continue;
        }
        if (!XzDiscOverlapsAabb(feet.x, feet.z, r, body.position.x, body.position.z,
                                body.halfExtents.x, body.halfExtents.z, -0.02f)) {
            continue;
        }

        if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triangleMeshes_.size() &&
            triangleMeshes_[bi].IsValid()) {
            // Vertical probe: walkable triangle tops under the capsule disc (ComplexAsSimple).
            const float rayTop =
                std::max(feet.y + stepUp + skin + 0.5f, body.position.y + body.halfExtents.y + 0.5f);
            const glm::vec3 start{feet.x, rayTop, feet.z};
            const glm::vec3 end{feet.x, floorY - 1.0f, feet.z};
            float t = 1.0f;
            glm::vec3 normal{};
            if (SegmentTriangleMesh(start, end, triangleMeshes_[bi], 0.0f, t, normal) &&
                normal.y > 0.15f) {
                const float yHit = start.y + ((end.y - start.y) * t);
                if (yHit <= feet.y + stepUp + skin) {
                    support = std::max(support, yHit);
                }
            }
            continue;
        }

        const float top = body.position.y + body.halfExtents.y;
        // Skip tops too high to step onto (side collision handles walls).
        if (feet.y + stepUp + skin < top) {
            continue;
        }
        support = std::max(support, top);
    }

    for (const SlopePlane& plane : slopePlanes_) {
        if (std::abs(plane.normal.y) < 1.0e-4f) {
            continue;
        }
        // Plane height at feet XZ: dot((x,y,z)-point, n) = 0.
        const float yOnPlane =
            plane.point.y -
            ((plane.normal.x * (feet.x - plane.point.x)) + (plane.normal.z * (feet.z - plane.point.z))) /
                plane.normal.y;
        if (feet.y + stepUp + skin < yOnPlane) {
            continue;
        }
        if (!XzDiscOverlapsAabb(feet.x, feet.z, r, plane.boundsCenter.x, plane.boundsCenter.z,
                                plane.boundsHalfExtents.x, plane.boundsHalfExtents.z, -0.02f)) {
            continue;
        }
        support = std::max(support, yOnPlane);
    }
    return support;
}

void PhysScene::ResolveCapsuleSides(const CapsuleShape& capsule, glm::vec3& feet,
                                    const glm::vec2& wishXZ, const CapsuleContactParams& params,
                                    std::size_t skipLevelMeshIndex, bool applyPush) {
    const float r = capsule.radius;
    const float feetY = feet.y;
    const float head = feetY + capsule.height;
    const bool hasWish = glm::length(wishXZ) > 1.0e-4f;
    const glm::vec2 wishN = hasWish ? glm::normalize(wishXZ) : glm::vec2{0.0f};

    for (BodyInstance& body : bodies_) {
        if (body.levelMeshIndex == skipLevelMeshIndex) {
            continue;
        }
        // ComplexAsSimple: sides come from TriangleMesh traces; world AABB is too fat for ramps.
        if (body.collisionShape == ECollisionShape::TriangleMesh) {
            continue;
        }
        const float hx = body.halfExtents.x;
        const float hy = body.halfExtents.y;
        const float hz = body.halfExtents.z;
        const float top = body.position.y + hy;
        const float bottom = body.position.y - hy;

        if (head < bottom) {
            continue;
        }

        const bool xzOnTop =
            XzDiscOverlapsAabb(feet.x, feet.z, r, body.position.x, body.position.z, hx, hz, -0.02f);
        // Standing on this top — no side push.
        if (feetY >= top - params.skin && xzOnTop) {
            continue;
        }
        // Airborne over the volume (jump/clearance) — no side push.
        // Still resolve when elevated *beside* a short ledge (step-up clearance).
        if (feetY > top && xzOnTop) {
            continue;
        }

        glm::vec2 normal{};
        float penetration = 0.0f;
        if (!CapsuleAabbMtv(feet.x, feet.z, r, body.position.x, body.position.z, hx, hz, normal,
                            penetration)) {
            continue;
        }

        if (body.type == EBodyType::Static) {
            feet.x += normal.x * penetration;
            feet.z += normal.y * penetration;
            ClampPositionXZ(feet, params.walkBounds);
            continue;
        }

        // Dynamic: mass-weighted depenetration (player ≈ fixed mass 80 for share).
        constexpr float kPlayerMass = 80.0f;
        const float bodyMass = std::max(body.mass, 0.5f);
        const float invSum = 1.0f / (kPlayerMass + bodyMass);
        const float playerShare = bodyMass * invSum;
        const float bodyShare = kPlayerMass * invSum;

        feet.x += normal.x * (penetration * playerShare);
        feet.z += normal.y * (penetration * playerShare);
        body.position.x -= normal.x * (penetration * bodyShare);
        body.position.z -= normal.y * (penetration * bodyShare);
        ClampPositionXZ(feet, params.walkBounds);
        ClampPositionXZ(body.position, params.walkBounds);

        cancelVelocityInto(body.velXZ, -normal);

        if (!applyPush || !hasWish) {
            continue;
        }

        applyDynamicWishPush(body, wishN, normal, params.pushStrength, params.walkBounds, false);
    }
}

bool PhysScene::ApplyCapsuleSweepPush(std::size_t levelMeshIndex, const glm::vec2& wishXZ,
                                      const glm::vec3& impactNormal, float pushStrength,
                                      float walkBounds) {
    if (levelMeshIndex == Level::npos || glm::length(wishXZ) <= 1.0e-4f) {
        return false;
    }

    glm::vec2 normal{impactNormal.x, impactNormal.z};
    const float nLen = glm::length(normal);
    if (nLen <= 1.0e-4f) {
        return false;
    }
    normal /= nLen;
    const glm::vec2 wishN = glm::normalize(wishXZ);

    for (BodyInstance& body : bodies_) {
        if (body.levelMeshIndex != levelMeshIndex || body.type != EBodyType::Dynamic) {
            continue;
        }
        const float into = std::max(0.0f, -glm::dot(wishN, normal));
        if (into <= 1.0e-4f) {
            return false;
        }
        applyDynamicWishPush(body, wishN, normal, pushStrength, walkBounds, true);
        return true;
    }
    return false;
}

void PhysScene::Step(const PhysSceneStepParams& params) {
    if (backendIface_ != nullptr && backendIface_->HasRigidWorld()) {
        backendIface_->RigidPrepareStep(bodies_, params.skipLevelMeshIndex);
        backendIface_->RigidStep(params.deltaTime, params.gravity, params.floorY);
        backendIface_->RigidReadBack(bodies_);
        for (BodyInstance& body : bodies_) {
            if (body.type != EBodyType::Dynamic) {
                continue;
            }
            ClampPositionXZ(body.position, params.walkBounds);
        }
        return;
    }

    const float damp = std::exp(-params.damping * params.deltaTime);

    auto supportUnderAabb = [&](const BodyInstance& body) -> float {
        float support = params.floorY;
        const float bx0 = body.position.x - body.halfExtents.x;
        const float bx1 = body.position.x + body.halfExtents.x;
        const float bz0 = body.position.z - body.halfExtents.z;
        const float bz1 = body.position.z + body.halfExtents.z;
        const float bottom = body.position.y - body.halfExtents.y;

        for (const BodyInstance& other : bodies_) {
            if (other.levelMeshIndex == body.levelMeshIndex ||
                other.levelMeshIndex == params.skipLevelMeshIndex) {
                continue;
            }
            const float ox0 = other.position.x - other.halfExtents.x;
            const float ox1 = other.position.x + other.halfExtents.x;
            const float oz0 = other.position.z - other.halfExtents.z;
            const float oz1 = other.position.z + other.halfExtents.z;
            if (bx1 < ox0 || bx0 > ox1 || bz1 < oz0 || bz0 > oz1) {
                continue;
            }

            const float top = other.position.y + other.halfExtents.y;
            // Dynamic support only when this body is clearly above the other (stacking).
            if (other.type == EBodyType::Dynamic && bottom + params.skin < top - 0.02f &&
                body.position.y <= other.position.y) {
                continue;
            }
            support = std::max(support, top);
        }
        return support;
    };

    // 1) Integrate velocities (no floor snap yet).
    for (BodyInstance& body : bodies_) {
        if (body.type != EBodyType::Dynamic) {
            body.velXZ = {};
            body.velocityY = 0.0f;
            continue;
        }

        if (body.enableGravity) {
            body.velocityY -= params.gravity * params.deltaTime;
            body.position.y += body.velocityY * params.deltaTime;
        } else {
            body.velocityY = 0.0f;
        }

        if (glm::length(body.velXZ) >= 1.0e-3f) {
            body.position.x += body.velXZ.x * params.deltaTime;
            body.position.z += body.velXZ.y * params.deltaTime;
            ClampPositionXZ(body.position, params.walkBounds);
            body.velXZ *= damp;
        } else {
            body.velXZ = {};
        }
    }

    // 2) Resolve overlaps on min-penetration axis (XZ sides vs Y stacking).
    constexpr int kIterations = 6;
    for (int iter = 0; iter < kIterations; ++iter) {
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            if (bodies_[i].levelMeshIndex == params.skipLevelMeshIndex) {
                continue;
            }
            for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
                if (bodies_[j].levelMeshIndex == params.skipLevelMeshIndex) {
                    continue;
                }

                BodyInstance& a = bodies_[i];
                BodyInstance& b = bodies_[j];

                const bool aDyn = a.type == EBodyType::Dynamic;
                const bool bDyn = b.type == EBodyType::Dynamic;
                if (!aDyn && !bDyn) {
                    continue;
                }

                float moveA = 0.0f;
                float moveB = 0.0f;
                if (aDyn && bDyn) {
                    const float sum = std::max(a.mass + b.mass, 1.0e-3f);
                    moveA = b.mass / sum;
                    moveB = a.mass / sum;
                } else if (aDyn) {
                    moveA = 1.0f;
                } else {
                    moveB = 1.0f;
                }

                glm::vec3 normal{};
                if (!SeparateAabb(a.position, a.halfExtents, b.position, b.halfExtents, moveA,
                                  moveB, &normal)) {
                    continue;
                }

                ClampPositionXZ(a.position, params.walkBounds);
                ClampPositionXZ(b.position, params.walkBounds);

                if (aDyn) {
                    cancelVelocityInto(a.velXZ, {normal.x, normal.z});
                    cancelVelocityYInto(a.velocityY, normal.y);
                }
                if (bDyn) {
                    cancelVelocityInto(b.velXZ, {-normal.x, -normal.z});
                    cancelVelocityYInto(b.velocityY, -normal.y);
                }
            }
        }
    }

    // 3) Floor / platform snap only when landing from above.
    for (BodyInstance& body : bodies_) {
        if (body.type != EBodyType::Dynamic || !body.enableGravity) {
            continue;
        }

        const float support = supportUnderAabb(body);
        const float bottom = body.position.y - body.halfExtents.y;
        const float window = landWindow(body.velocityY, params.deltaTime, params.skin);
        if (body.velocityY <= 0.0f && bottom <= support + params.skin &&
            bottom >= support - window) {
            body.position.y = support + body.halfExtents.y;
            body.velocityY = 0.0f;
            // Resting friction: kill tiny residual slide when fully supported.
            if (glm::length(body.velXZ) < 0.08f) {
                body.velXZ = {};
            }
        }
    }
}

void PhysScene::AppendCollisionDebug(DebugDraw& draw, const CapsuleShape& capsule,
                                     const glm::vec3& feet, std::size_t skipLevelMeshIndex) const {
    const float r = capsule.radius;
    const float h = capsule.height;
    const float cylBottom = std::min(r, h * 0.5f);
    const float cylTop = std::max(h - r, cylBottom);
    constexpr glm::vec3 kCapsuleColor{0.2f, 1.0f, 0.45f};
    constexpr int kSeg = 12;

    const glm::vec3 b0 = feet + glm::vec3{0.0f, cylBottom, 0.0f};
    const glm::vec3 t0 = feet + glm::vec3{0.0f, cylTop, 0.0f};
    draw.AddLine(b0 + glm::vec3{r, 0, 0}, t0 + glm::vec3{r, 0, 0}, kCapsuleColor);
    draw.AddLine(b0 + glm::vec3{-r, 0, 0}, t0 + glm::vec3{-r, 0, 0}, kCapsuleColor);
    draw.AddLine(b0 + glm::vec3{0, 0, r}, t0 + glm::vec3{0, 0, r}, kCapsuleColor);
    draw.AddLine(b0 + glm::vec3{0, 0, -r}, t0 + glm::vec3{0, 0, -r}, kCapsuleColor);

    appendCapsuleRing(draw, feet + glm::vec3{0.0f, cylBottom, 0.0f}, r, kCapsuleColor, kSeg);
    appendCapsuleRing(draw, feet + glm::vec3{0.0f, cylTop, 0.0f}, r, kCapsuleColor, kSeg);

    if (cylBottom > 1.0e-3f) {
        appendCapsuleRing(draw, feet + glm::vec3{0.0f, cylBottom * 0.5f, 0.0f}, r * 0.85f,
                          kCapsuleColor, kSeg / 2);
    }
    if (h - cylTop > 1.0e-3f) {
        const float capMidY = cylTop + ((h - cylTop) * 0.5f);
        appendCapsuleRing(draw, feet + glm::vec3{0.0f, capMidY, 0.0f}, r * 0.85f, kCapsuleColor,
                          kSeg / 2);
    }
    appendCapsuleRing(draw, feet, r * 0.35f, kCapsuleColor, 6);
    appendCapsuleRing(draw, feet + glm::vec3{0.0f, h, 0.0f}, r * 0.35f, kCapsuleColor, 6);

    AppendBodiesCollisionDebug(draw, skipLevelMeshIndex);
}

void PhysScene::AppendBodiesCollisionDebug(DebugDraw& draw,
                                           std::size_t skipLevelMeshIndex) const {
    constexpr glm::vec3 kDynamicColor{1.0f, 0.55f, 0.15f};
    constexpr glm::vec3 kStaticColor{0.35f, 0.65f, 1.0f};
    constexpr glm::vec3 kTriMeshColor{0.25f, 0.9f, 1.0f};
    for (std::size_t bi = 0; bi < bodies_.size(); ++bi) {
        const BodyInstance& body = bodies_[bi];
        if (body.levelMeshIndex == skipLevelMeshIndex) {
            continue;
        }

        // TriangleMesh: draw actual tris (oriented). AABB alone looks like a fat unrotated box.
        if (body.collisionShape == ECollisionShape::TriangleMesh && bi < triangleMeshes_.size() &&
            triangleMeshes_[bi].IsValid()) {
            const TriangleMeshCollision& mesh = triangleMeshes_[bi];
            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const glm::vec3& v0 = mesh.positions[mesh.indices[i]];
                const glm::vec3& v1 = mesh.positions[mesh.indices[i + 1]];
                const glm::vec3& v2 = mesh.positions[mesh.indices[i + 2]];
                draw.AddLine(v0, v1, kTriMeshColor);
                draw.AddLine(v1, v2, kTriMeshColor);
                draw.AddLine(v2, v0, kTriMeshColor);
            }
            continue;
        }

        const glm::vec3 mn = body.position - body.halfExtents;
        const glm::vec3 mx = body.position + body.halfExtents;
        draw.AddAabb(mn, mx, body.type == EBodyType::Dynamic ? kDynamicColor : kStaticColor);
    }

    // Walkable slope planes (AddSlopeRamp) — magenta wire quads for F2.
    constexpr glm::vec3 kSlopeColor{0.95f, 0.2f, 0.85f};
    for (const SlopePlane& plane : slopePlanes_) {
        const float hx = plane.boundsHalfExtents.x;
        const float hz = plane.boundsHalfExtents.z;
        const glm::vec3& c = plane.boundsCenter;
        const float nLen = glm::length(plane.normal);
        if (nLen < 1.0e-6f || std::abs(plane.normal.y) < 1.0e-4f) {
            continue;
        }
        const glm::vec3 n = plane.normal / nLen;
        auto yAt = [&](float x, float z) {
            return plane.point.y -
                   ((n.x * (x - plane.point.x)) + (n.z * (z - plane.point.z))) / n.y;
        };
        const glm::vec3 p00{c.x - hx, yAt(c.x - hx, c.z - hz), c.z - hz};
        const glm::vec3 p10{c.x + hx, yAt(c.x + hx, c.z - hz), c.z - hz};
        const glm::vec3 p11{c.x + hx, yAt(c.x + hx, c.z + hz), c.z + hz};
        const glm::vec3 p01{c.x - hx, yAt(c.x - hx, c.z + hz), c.z + hz};
        draw.AddLine(p00, p10, kSlopeColor);
        draw.AddLine(p10, p11, kSlopeColor);
        draw.AddLine(p11, p01, kSlopeColor);
        draw.AddLine(p01, p00, kSlopeColor);
        draw.AddLine(p00, p11, kSlopeColor);
        draw.AddLine(p10, p01, kSlopeColor);
    }
}

} // namespace leon
