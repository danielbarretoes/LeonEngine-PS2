#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Math/Transform.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Transform identity modelMatrix", "[core][transform]") {
    leon::Transform t;
    const glm::mat4 m = t.modelMatrix();
    REQUIRE_THAT(m[0][0], WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(m[1][1], WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(m[2][2], WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(m[3][0], WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(m[3][1], WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(m[3][2], WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("Transform applies translation and yaw", "[core][transform]") {
    leon::Transform t;
    t.position = {2.0f, 3.0f, 4.0f};
    t.rotationDegrees = {0.0f, 90.0f, 0.0f};

    const glm::mat4 m = t.modelMatrix();
    REQUIRE_THAT(m[3].x, WithinAbs(2.0f, 1.0e-4f));
    REQUIRE_THAT(m[3].y, WithinAbs(3.0f, 1.0e-4f));
    REQUIRE_THAT(m[3].z, WithinAbs(4.0f, 1.0e-4f));
    // 90° yaw: local +X → world -Z
    REQUIRE_THAT(m[0].x, WithinAbs(0.0f, 1.0e-4f));
    REQUIRE_THAT(m[0].z, WithinAbs(-1.0f, 1.0e-4f));
}

TEST_CASE("Transform sanitizes near-zero scale", "[core][transform]") {
    leon::Transform t;
    t.scale = {0.0f, 1.0f, 0.0f};
    const glm::mat4 m = t.modelMatrix();
    REQUIRE(std::abs(m[0][0]) >= 1.0e-4f);
    REQUIRE(std::abs(m[2][2]) >= 1.0e-4f);
    // normalMatrix should remain finite / non-singular enough
    const glm::mat3 n = t.normalMatrix();
    REQUIRE(std::isfinite(n[0][0]));
    REQUIRE(std::isfinite(n[1][1]));
}
