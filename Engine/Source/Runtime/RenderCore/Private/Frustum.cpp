#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include "Frustum.h"
#include <limits>
#include <utility>

namespace {

struct RawPlane {
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;
};

RawPlane normalizePlane(float a, float b, float c, float d) {
    const float len = std::sqrt((a * a) + (b * b) + (c * c));
    if (len > 1e-8f) {
        const float inv = 1.0f / len;
        return {.a = a * inv, .b = b * inv, .c = c * inv, .d = d * inv};
    }
    return {.a = 0.0f, .b = 1.0f, .c = 0.0f, .d = 0.0f};
}

} // namespace

Aabb Aabb::fromLocalTransformed(const glm::vec3& localMin, const glm::vec3& localMax,
                                const glm::mat4& model) {
    const std::array<glm::vec3, 8> corners = {{
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    }};

    Aabb box;
    box.min = glm::vec3(std::numeric_limits<float>::max());
    box.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (const glm::vec3& local : corners) {
        const glm::vec3 world = glm::vec3(model * glm::vec4(local, 1.0f));
        box.min = glm::min(box.min, world);
        box.max = glm::max(box.max, world);
    }
    return box;
}

bool Aabb::intersectRay(const glm::vec3& origin, const glm::vec3& dir, float& outT) const {
    constexpr float kEps = 1.0e-8f;
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        const float o = origin[axis];
        const float d = dir[axis];
        const float bMin = min[axis];
        const float bMax = max[axis];
        if (std::abs(d) < kEps) {
            if (o < bMin || o > bMax) {
                return false;
            }
            continue;
        }
        float t0 = (bMin - o) / d;
        float t1 = (bMax - o) / d;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMin > tMax) {
            return false;
        }
    }

    if (tMax < 0.0f) {
        return false;
    }
    outT = tMin >= 0.0f ? tMin : tMax;
    return outT >= 0.0f;
}

void Frustum::extractFromViewProjection(const glm::mat4& viewProjection) {
    // Gribb/Hartmann: combine clip-matrix columns into frustum planes.
    const glm::mat4& m = viewProjection;
    const std::array<RawPlane, 6> raw = {{
        normalizePlane(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0],
                       m[3][3] + m[3][0]), // left
        normalizePlane(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0],
                       m[3][3] - m[3][0]), // right
        normalizePlane(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1],
                       m[3][3] + m[3][1]), // bottom
        normalizePlane(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1],
                       m[3][3] - m[3][1]), // top
        normalizePlane(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2],
                       m[3][3] + m[3][2]), // near
        normalizePlane(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2],
                       m[3][3] - m[3][2]), // far
    }};

    auto assign = [](Plane& dst, const RawPlane& src) {
        dst.normal = {src.a, src.b, src.c};
        dst.distance = src.d;
    };
    assign(planes_[0], raw[0]);
    assign(planes_[1], raw[1]);
    assign(planes_[2], raw[2]);
    assign(planes_[3], raw[3]);
    assign(planes_[4], raw[4]);
    assign(planes_[5], raw[5]);
}

bool Frustum::intersectsAabb(const Aabb& box) const {
    for (const Plane& plane : planes_) {
        const glm::vec3 positive{
            plane.normal.x >= 0.0f ? box.max.x : box.min.x,
            plane.normal.y >= 0.0f ? box.max.y : box.min.y,
            plane.normal.z >= 0.0f ? box.max.z : box.min.z,
        };
        if ((glm::dot(plane.normal, positive) + plane.distance) < 0.0f) {
            return false;
        }
    }
    return true;
}

