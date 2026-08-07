#include <leon/physics/TriangleCollision.h>

#include <glm/geometric.hpp>

#include <cmath>

namespace leon {
namespace {

[[nodiscard]] bool pointInTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b,
                                   const glm::vec3& c, const glm::vec3& normal) {
    const glm::vec3 n = normal;
    const glm::vec3 edge0 = b - a;
    const glm::vec3 edge1 = c - b;
    const glm::vec3 edge2 = a - c;
    if (glm::dot(n, glm::cross(edge0, p - a)) < -1.0e-5f) {
        return false;
    }
    if (glm::dot(n, glm::cross(edge1, p - b)) < -1.0e-5f) {
        return false;
    }
    if (glm::dot(n, glm::cross(edge2, p - c)) < -1.0e-5f) {
        return false;
    }
    return true;
}

} // namespace

bool SegmentTriangle(const glm::vec3& start, const glm::vec3& end, const glm::vec3& v0,
                     const glm::vec3& v1, const glm::vec3& v2, float& outT, glm::vec3& outNormal) {
    const glm::vec3 edge1 = v1 - v0;
    const glm::vec3 edge2 = v2 - v0;
    glm::vec3 normal = glm::cross(edge1, edge2);
    const float nLen = glm::length(normal);
    if (nLen < 1.0e-8f) {
        return false;
    }
    normal /= nLen;

    const glm::vec3 dir = end - start;
    const float denom = glm::dot(normal, dir);
    if (std::abs(denom) < 1.0e-8f) {
        return false;
    }
    const float t = glm::dot(normal, v0 - start) / denom;
    if (t < 0.0f || t > 1.0f) {
        return false;
    }
    const glm::vec3 hit = start + (dir * t);
    if (!pointInTriangle(hit, v0, v1, v2, normal)) {
        return false;
    }
    outT = t;
    // Face the incoming ray (Unreal blocking normal points toward the tracer).
    outNormal = (denom < 0.0f) ? normal : -normal;
    return true;
}

bool SegmentTriangleInflated(const glm::vec3& start, const glm::vec3& end, const glm::vec3& v0,
                             const glm::vec3& v1, const glm::vec3& v2, float inflate, float& outT,
                             glm::vec3& outNormal) {
    const glm::vec3 edge1 = v1 - v0;
    const glm::vec3 edge2 = v2 - v0;
    glm::vec3 normal = glm::cross(edge1, edge2);
    const float nLen = glm::length(normal);
    if (nLen < 1.0e-8f) {
        return false;
    }
    normal /= nLen;

    const float pad = std::max(inflate, 0.0f);
    // Offset plane toward the start of the segment (sphere center approach).
    const float dStart = glm::dot(start - v0, normal);
    const glm::vec3 planeN = (dStart >= 0.0f) ? normal : -normal;
    const glm::vec3 planePoint = v0 + (planeN * pad);

    const glm::vec3 dir = end - start;
    const float denom = glm::dot(planeN, dir);
    if (std::abs(denom) < 1.0e-8f) {
        return false;
    }
    const float t = glm::dot(planeN, planePoint - start) / denom;
    if (t < 0.0f || t > 1.0f) {
        return false;
    }
    const glm::vec3 hit = start + (dir * t);
    const glm::vec3 onTri = hit - (planeN * pad);
    if (!pointInTriangle(onTri, v0, v1, v2, normal)) {
        return false;
    }
    outT = t;
    outNormal = planeN;
    return true;
}

bool SegmentTriangleMesh(const glm::vec3& start, const glm::vec3& end,
                         const TriangleMeshCollision& mesh, float inflate, float& outT,
                         glm::vec3& outNormal) {
    if (!mesh.IsValid()) {
        return false;
    }
    bool any = false;
    float bestT = 1.0f;
    glm::vec3 bestN{0.0f, 1.0f, 0.0f};
    const std::size_t triCount = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t i0 = mesh.indices[t * 3 + 0];
        const std::uint32_t i1 = mesh.indices[t * 3 + 1];
        const std::uint32_t i2 = mesh.indices[t * 3 + 2];
        if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() ||
            i2 >= mesh.positions.size()) {
            continue;
        }
        float hitT = 1.0f;
        glm::vec3 hitN{};
        const bool ok = (inflate > 1.0e-6f)
                            ? SegmentTriangleInflated(start, end, mesh.positions[i0],
                                                      mesh.positions[i1], mesh.positions[i2],
                                                      inflate, hitT, hitN)
                            : SegmentTriangle(start, end, mesh.positions[i0], mesh.positions[i1],
                                              mesh.positions[i2], hitT, hitN);
        if (!ok || hitT > bestT) {
            continue;
        }
        bestT = hitT;
        bestN = hitN;
        any = true;
    }
    if (!any) {
        return false;
    }
    outT = bestT;
    outNormal = bestN;
    return true;
}

} // namespace leon
