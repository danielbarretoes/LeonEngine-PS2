#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>


struct FCapsuleShape {
    float Radius = 0.35f;
    /// Full vertical extent from feet to top (cylinder + end caps approximated in XZ).
    float Height = 1.85f;
};

void HalfExtentsFromScale(const glm::vec3& Scale, float& HalfX, float& HalfY, float& HalfZ);

[[nodiscard]] float MassFromHalfExtents(float HalfX, float HalfY, float HalfZ);

void ClampPositionXZ(glm::vec3& Pos, float Bounds);

[[nodiscard]] bool XzDiscOverlapsAabb(float X, float Z, float InRadius, float Cx, float Cz, float Hx,
                                      float Hz, float Inflate);

/// Capsule (XZ disc) vs AABB: outward normal (cube → capsule) and penetration.
[[nodiscard]] bool CapsuleAabbMtv(float Px, float Pz, float InRadius, float Cx, float Cz, float Hx,
                                  float Hz, glm::vec2& OutNormal, float& OutPenetration);

[[nodiscard]] bool AabbOverlapY(float Ay, float Ahy, float By, float Bhy);

/// Separate two XZ AABBs. moveA/moveB are MTV shares (static → 0).
[[nodiscard]] bool SeparateAabbXZ(glm::vec3& A, float Ahx, float Ahz, glm::vec3& B, float Bhx,
                                  float Bhz, float MoveA, float MoveB);

/// Separate two AABBs on the minimum-penetration axis (X, Y, or Z).
/// `outNormal` is unit MTV direction a←b when non-null. moveA/moveB are shares (static → 0).
[[nodiscard]] bool SeparateAabb(glm::vec3& A, const glm::vec3& AHalfExtents, glm::vec3& B,
                                const glm::vec3& bHalfExtents, float MoveA, float MoveB,
                                glm::vec3* OutNormal = nullptr);

