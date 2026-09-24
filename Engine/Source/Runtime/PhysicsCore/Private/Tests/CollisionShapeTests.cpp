#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "CollisionShape.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("HalfExtentsFromScale uses absolute half scale", "[physics][collision]") {
    float hx = 0.0f, hy = 0.0f, hz = 0.0f;
    HalfExtentsFromScale({2.0f, -4.0f, 6.0f}, hx, hy, hz);
    REQUIRE_THAT(hx, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(hy, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(hz, WithinAbs(3.0f, 1.0e-5f));
}

TEST_CASE("MassFromHalfExtents floors tiny volumes", "[physics][collision]") {
    REQUIRE(MassFromHalfExtents(0.01f, 0.01f, 0.01f) >= 0.08f);
    REQUIRE_THAT(MassFromHalfExtents(1.0f, 1.0f, 1.0f), WithinAbs(8.0f, 1.0e-5f));
}

TEST_CASE("ClampPositionXZ clamps to bounds", "[physics][collision]") {
    glm::vec3 p{100.0f, 5.0f, -50.0f};
    ClampPositionXZ(p, 18.0f);
    REQUIRE_THAT(p.x, WithinAbs(18.0f, 1.0e-5f));
    REQUIRE_THAT(p.y, WithinAbs(5.0f, 1.0e-5f));
    REQUIRE_THAT(p.z, WithinAbs(-18.0f, 1.0e-5f));
}

TEST_CASE("XzDiscOverlapsAabb detects overlap", "[physics][collision]") {
    REQUIRE(XzDiscOverlapsAabb(0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f));
    REQUIRE_FALSE(XzDiscOverlapsAabb(5.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f));
}

TEST_CASE("CapsuleAabbMtv pushes capsule out of AABB", "[physics][collision]") {
    glm::vec2 normal{};
    float penetration = 0.0f;
    REQUIRE(CapsuleAabbMtv(0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.4f, 0.4f, normal, penetration));
    REQUIRE(penetration > 0.0f);
    REQUIRE(glm::length(normal) > 0.5f);

    REQUIRE_FALSE(
        CapsuleAabbMtv(3.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.4f, 0.4f, normal, penetration));
}

TEST_CASE("SeparateAabb separates overlapping boxes", "[physics][collision]") {
    glm::vec3 a{0.0f, 0.0f, 0.0f};
    glm::vec3 b{0.5f, 0.0f, 0.0f};
    const glm::vec3 half{0.5f, 0.5f, 0.5f};
    glm::vec3 normal{};
    REQUIRE(SeparateAabb(a, half, b, half, 0.5f, 0.5f, &normal));
    // Centers should move apart along X
    REQUIRE(a.x < 0.0f);
    REQUIRE(b.x > 0.5f);
}
