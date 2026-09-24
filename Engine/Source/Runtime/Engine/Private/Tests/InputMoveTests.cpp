#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Camera/Camera.h"
#include "GameFramework/Input.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("MoveAxes2D any detects nonzero", "[core][input]") {
    MoveAxes2D zero{};
    REQUIRE_FALSE(zero.any());
    REQUIRE(MoveAxes2D{1.0f, 0.0f}.any());
    REQUIRE(MoveAxes2D{0.0f, -1.0f}.any());
}

TEST_CASE("yawRelativeMoveXZ returns zero without axes", "[core][input]") {
    const glm::vec3 move = yawRelativeMoveXZ(45.0f, {});
    REQUIRE_THAT(move.x, WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_THAT(move.y, WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_THAT(move.z, WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("yawRelativeMoveXZ forward at yaw 0", "[core][input]") {
    const glm::vec3 move = yawRelativeMoveXZ(0.0f, {0.0f, 1.0f});
    REQUIRE_THAT(glm::length(move), WithinAbs(1.0f, 1.0e-4f));
    REQUIRE_THAT(move.x, WithinAbs(-1.0f, 1.0e-4f));
    REQUIRE_THAT(move.y, WithinAbs(0.0f, 1.0e-4f));
    REQUIRE_THAT(move.z, WithinAbs(0.0f, 1.0e-4f));
}

TEST_CASE("cameraRelativeMoveXZ matches camera yaw", "[core][input]") {
    Camera cam;
    cam.SetYawPitch(90.0f, 0.0f);
    const glm::vec3 a = cameraRelativeMoveXZ(cam, {0.0f, 1.0f});
    const glm::vec3 b = yawRelativeMoveXZ(90.0f, {0.0f, 1.0f});
    REQUIRE_THAT(a.x, WithinAbs(b.x, 1.0e-5f));
    REQUIRE_THAT(a.z, WithinAbs(b.z, 1.0e-5f));
}
