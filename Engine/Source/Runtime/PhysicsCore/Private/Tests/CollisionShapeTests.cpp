#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "CollisionShape.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("HalfExtentsFromScale uses absolute half scale", "[physics][collision]") {
    float Hx = 0.0f, Hy = 0.0f, Hz = 0.0f;
    HalfExtentsFromScale({2.0f, -4.0f, 6.0f}, Hx, Hy, Hz);
    REQUIRE_THAT(Hx, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(Hy, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(Hz, WithinAbs(3.0f, 1.0e-5f));
}

TEST_CASE("MassFromHalfExtents floors tiny volumes", "[physics][collision]") {
    REQUIRE(MassFromHalfExtents(0.01f, 0.01f, 0.01f) >= 0.08f);
    REQUIRE_THAT(MassFromHalfExtents(1.0f, 1.0f, 1.0f), WithinAbs(8.0f, 1.0e-5f));
}

TEST_CASE("ClampPositionXZ clamps to bounds", "[physics][collision]") {
    glm::vec3 P{100.0f, 5.0f, -50.0f};
    ClampPositionXZ(P, 18.0f);
    REQUIRE_THAT(P.x, WithinAbs(18.0f, 1.0e-5f));
    REQUIRE_THAT(P.y, WithinAbs(5.0f, 1.0e-5f));
    REQUIRE_THAT(P.z, WithinAbs(-18.0f, 1.0e-5f));
}

TEST_CASE("XzDiscOverlapsAabb detects overlap", "[physics][collision]") {
    REQUIRE(XzDiscOverlapsAabb(0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f));
    REQUIRE_FALSE(XzDiscOverlapsAabb(5.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f));
}

TEST_CASE("CapsuleAabbMtv pushes capsule out of AABB", "[physics][collision]") {
    glm::vec2 Normal{};
    float Penetration = 0.0f;
    REQUIRE(CapsuleAabbMtv(0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.4f, 0.4f, Normal, Penetration));
    REQUIRE(Penetration > 0.0f);
    REQUIRE(glm::length(Normal) > 0.5f);

    REQUIRE_FALSE(
        CapsuleAabbMtv(3.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.4f, 0.4f, Normal, Penetration));
}

TEST_CASE("SeparateAabb separates overlapping boxes", "[physics][collision]") {
    glm::vec3 A{0.0f, 0.0f, 0.0f};
    glm::vec3 B{0.5f, 0.0f, 0.0f};
    const glm::vec3 Half{0.5f, 0.5f, 0.5f};
    glm::vec3 Normal{};
    REQUIRE(SeparateAabb(A, Half, B, Half, 0.5f, 0.5f, &Normal));
    // Centers should move apart along X
    REQUIRE(A.x < 0.0f);
    REQUIRE(B.x > 0.5f);
}
