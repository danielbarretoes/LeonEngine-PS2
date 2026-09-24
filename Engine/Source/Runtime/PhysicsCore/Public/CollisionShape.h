#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>


struct FCapsuleShape {
    float radius = 0.35f;
    /// Full vertical extent from feet to top (cylinder + end caps approximated in XZ).
    float height = 1.85f;
};

void HalfExtentsFromScale(const glm::vec3& scale, float& halfX, float& halfY, float& halfZ);

[[nodiscard]] float MassFromHalfExtents(float halfX, float halfY, float halfZ);

void ClampPositionXZ(glm::vec3& pos, float bounds);

[[nodiscard]] bool XzDiscOverlapsAabb(float x, float z, float radius, float cx, float cz, float hx,
                                      float hz, float inflate);

/// Capsule (XZ disc) vs AABB: outward normal (cube → capsule) and penetration.
[[nodiscard]] bool CapsuleAabbMtv(float px, float pz, float radius, float cx, float cz, float hx,
                                  float hz, glm::vec2& outNormal, float& outPenetration);

[[nodiscard]] bool AabbOverlapY(float ay, float ahy, float by, float bhy);

/// Separate two XZ AABBs. moveA/moveB are MTV shares (static → 0).
[[nodiscard]] bool SeparateAabbXZ(glm::vec3& a, float ahx, float ahz, glm::vec3& b, float bhx,
                                  float bhz, float moveA, float moveB);

/// Separate two AABBs on the minimum-penetration axis (X, Y, or Z).
/// `outNormal` is unit MTV direction a←b when non-null. moveA/moveB are shares (static → 0).
[[nodiscard]] bool SeparateAabb(glm::vec3& a, const glm::vec3& aHalfExtents, glm::vec3& b,
                                const glm::vec3& bHalfExtents, float moveA, float moveB,
                                glm::vec3* outNormal = nullptr);

