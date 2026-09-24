#include "TriangleCollision.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace {

[[nodiscard]] bool PointInTriangle(const glm::vec3& P, const glm::vec3& A, const glm::vec3& B,
                                   const glm::vec3& C, const glm::vec3& Normal) {
    const glm::vec3 N = Normal;
    const glm::vec3 Edge0 = B - A;
    const glm::vec3 Edge1 = C - B;
    const glm::vec3 Edge2 = A - C;
    if (glm::dot(N, glm::cross(Edge0, P - A)) < -1.0e-5f) {
        return false;
    }
    if (glm::dot(N, glm::cross(Edge1, P - B)) < -1.0e-5f) {
        return false;
    }
    if (glm::dot(N, glm::cross(Edge2, P - C)) < -1.0e-5f) {
        return false;
    }
    return true;
}

} // namespace

bool SegmentTriangle(const glm::vec3& Start, const glm::vec3& End, const glm::vec3& V0,
                     const glm::vec3& V1, const glm::vec3& V2, float& OutT, glm::vec3& OutNormal) {
    const glm::vec3 Edge1 = V1 - V0;
    const glm::vec3 Edge2 = V2 - V0;
    glm::vec3 Normal = glm::cross(Edge1, Edge2);
    const float NLen = glm::length(Normal);
    if (NLen < 1.0e-8f) {
        return false;
    }
    Normal /= NLen;

    const glm::vec3 Dir = End - Start;
    const float Denom = glm::dot(Normal, Dir);
    if (std::abs(Denom) < 1.0e-8f) {
        return false;
    }
    const float T = glm::dot(Normal, V0 - Start) / Denom;
    if (T < 0.0f || T > 1.0f) {
        return false;
    }
    const glm::vec3 Hit = Start + (Dir * T);
    if (!PointInTriangle(Hit, V0, V1, V2, Normal)) {
        return false;
    }
    OutT = T;
    // Face the incoming ray (Unreal blocking normal points toward the tracer).
    OutNormal = (Denom < 0.0f) ? Normal : -Normal;
    return true;
}

bool SegmentTriangleInflated(const glm::vec3& Start, const glm::vec3& End, const glm::vec3& V0,
                             const glm::vec3& V1, const glm::vec3& V2, float Inflate, float& OutT,
                             glm::vec3& OutNormal) {
    const glm::vec3 Edge1 = V1 - V0;
    const glm::vec3 Edge2 = V2 - V0;
    glm::vec3 Normal = glm::cross(Edge1, Edge2);
    const float NLen = glm::length(Normal);
    if (NLen < 1.0e-8f) {
        return false;
    }
    Normal /= NLen;

    const float Pad = std::max(Inflate, 0.0f);
    // Offset plane toward the start of the segment (sphere center approach).
    const float DStart = glm::dot(Start - V0, Normal);
    const glm::vec3 PlaneN = (DStart >= 0.0f) ? Normal : -Normal;
    const glm::vec3 PlanePoint = V0 + (PlaneN * Pad);

    const glm::vec3 Dir = End - Start;
    const float Denom = glm::dot(PlaneN, Dir);
    if (std::abs(Denom) < 1.0e-8f) {
        return false;
    }
    const float T = glm::dot(PlaneN, PlanePoint - Start) / Denom;
    if (T < 0.0f || T > 1.0f) {
        return false;
    }
    const glm::vec3 Hit = Start + (Dir * T);
    const glm::vec3 OnTri = Hit - (PlaneN * Pad);
    if (!PointInTriangle(OnTri, V0, V1, V2, Normal)) {
        return false;
    }
    OutT = T;
    OutNormal = PlaneN;
    return true;
}

bool SegmentTriangleMesh(const glm::vec3& Start, const glm::vec3& End,
                         const FTriangleMeshCollision& Mesh, float Inflate, float& OutT,
                         glm::vec3& OutNormal) {
    if (!Mesh.IsValid()) {
        return false;
    }
    bool bAny = false;
    float BestT = 1.0f;
    glm::vec3 BestN{0.0f, 1.0f, 0.0f};
    const std::size_t TriCount = Mesh.Indices.size() / 3;
    for (std::size_t T = 0; T < TriCount; ++T) {
        const std::uint32_t I0 = Mesh.Indices[T * 3 + 0];
        const std::uint32_t I1 = Mesh.Indices[T * 3 + 1];
        const std::uint32_t I2 = Mesh.Indices[T * 3 + 2];
        if (I0 >= Mesh.Positions.size() || I1 >= Mesh.Positions.size() ||
            I2 >= Mesh.Positions.size()) {
            continue;
        }
        float HitT = 1.0f;
        glm::vec3 HitN{};
        const bool bOk = (Inflate > 1.0e-6f)
                            ? SegmentTriangleInflated(Start, End, Mesh.Positions[I0],
                                                      Mesh.Positions[I1], Mesh.Positions[I2],
                                                      Inflate, HitT, HitN)
                            : SegmentTriangle(Start, End, Mesh.Positions[I0], Mesh.Positions[I1],
                                              Mesh.Positions[I2], HitT, HitN);
        if (!bOk || HitT > BestT) {
            continue;
        }
        BestT = HitT;
        BestN = HitN;
        bAny = true;
    }
    if (!bAny) {
        return false;
    }
    OutT = BestT;
    OutNormal = BestN;
    return true;
}

