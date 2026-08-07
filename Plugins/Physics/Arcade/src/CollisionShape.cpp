#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <leon/physics/CollisionShape.h>

namespace leon {

void HalfExtentsFromScale(const glm::vec3& scale, float& halfX, float& halfY, float& halfZ) {
    halfX = 0.5f * std::abs(scale.x);
    halfY = 0.5f * std::abs(scale.y);
    halfZ = 0.5f * std::abs(scale.z);
}

float MassFromHalfExtents(float halfX, float halfY, float halfZ) {
    return std::max(0.08f, 8.0f * halfX * halfY * halfZ);
}

void ClampPositionXZ(glm::vec3& pos, float bounds) {
    pos.x = std::clamp(pos.x, -bounds, bounds);
    pos.z = std::clamp(pos.z, -bounds, bounds);
}

bool XzDiscOverlapsAabb(float x, float z, float radius, float cx, float cz, float hx, float hz,
                        float inflate) {
    hx += inflate;
    hz += inflate;
    const float nearestX = std::clamp(x, cx - hx, cx + hx);
    const float nearestZ = std::clamp(z, cz - hz, cz + hz);
    const float dx = x - nearestX;
    const float dz = z - nearestZ;
    return ((dx * dx) + (dz * dz)) <= (radius * radius);
}

bool CapsuleAabbMtv(float px, float pz, float radius, float cx, float cz, float hx, float hz,
                    glm::vec2& outNormal, float& outPenetration) {
    const float dx = px - cx;
    const float dz = pz - cz;
    const float closestX = std::clamp(px, cx - hx, cx + hx);
    const float closestZ = std::clamp(pz, cz - hz, cz + hz);
    const float ox = px - closestX;
    const float oz = pz - closestZ;
    const float distSq = (ox * ox) + (oz * oz);

    if (distSq > 1.0e-8f) {
        const float dist = std::sqrt(distSq);
        if (dist >= radius) {
            return false;
        }
        outNormal = {ox / dist, oz / dist};
        outPenetration = radius - dist;
        return outPenetration > 0.0f;
    }

    const float overlapX = hx + radius - std::abs(dx);
    const float overlapZ = hz + radius - std::abs(dz);
    if (overlapX <= 0.0f || overlapZ <= 0.0f) {
        return false;
    }
    if (overlapX < overlapZ) {
        outNormal = {dx >= 0.0f ? 1.0f : -1.0f, 0.0f};
        outPenetration = overlapX;
    } else {
        outNormal = {0.0f, dz >= 0.0f ? 1.0f : -1.0f};
        outPenetration = overlapZ;
    }
    return true;
}

bool AabbOverlapY(float ay, float ahy, float by, float bhy) {
    return std::abs(ay - by) < (ahy + bhy);
}

bool SeparateAabbXZ(glm::vec3& a, float ahx, float ahz, glm::vec3& b, float bhx, float bhz,
                    float moveA, float moveB) {
    return SeparateAabb(a, {ahx, 1.0e6f, ahz}, b, {bhx, 1.0e6f, bhz}, moveA, moveB, nullptr);
}

bool SeparateAabb(glm::vec3& a, const glm::vec3& aHalfExtents, glm::vec3& b,
                  const glm::vec3& bHalfExtents, float moveA, float moveB, glm::vec3* outNormal) {
    const float overlapX = (aHalfExtents.x + bHalfExtents.x) - std::abs(a.x - b.x);
    const float overlapY = (aHalfExtents.y + bHalfExtents.y) - std::abs(a.y - b.y);
    const float overlapZ = (aHalfExtents.z + bHalfExtents.z) - std::abs(a.z - b.z);
    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) {
        return false;
    }

    const float share = moveA + moveB;
    if (share <= 1.0e-6f) {
        return false;
    }

    glm::vec3 mtv{0.0f};
    if (overlapX <= overlapY && overlapX <= overlapZ) {
        mtv.x = (a.x >= b.x ? 1.0f : -1.0f) * overlapX;
    } else if (overlapY <= overlapX && overlapY <= overlapZ) {
        mtv.y = (a.y >= b.y ? 1.0f : -1.0f) * overlapY;
    } else {
        mtv.z = (a.z >= b.z ? 1.0f : -1.0f) * overlapZ;
    }

    const float inv = 1.0f / share;
    a += mtv * (moveA * inv);
    b -= mtv * (moveB * inv);

    if (outNormal != nullptr) {
        const float len = glm::length(mtv);
        *outNormal = len > 1.0e-8f ? (mtv / len) : glm::vec3{0.0f, 1.0f, 0.0f};
    }
    return true;
}

} // namespace leon
