#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Camera/Camera.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Camera orbit clamps pitch", "[core][camera]") {
    leon::Camera cam;
    cam.SetYawPitch(0.0f, 0.0f);
    cam.Orbit(0.0f, 200.0f);
    REQUIRE_THAT(cam.PitchDegrees(), WithinAbs(89.0f, 1.0e-4f));
    cam.Orbit(0.0f, -400.0f);
    REQUIRE_THAT(cam.PitchDegrees(), WithinAbs(-89.0f, 1.0e-4f));
}

TEST_CASE("Camera orbit distance clamps and FreeLook ignores zoom", "[core][camera]") {
    leon::Camera cam;
    cam.SetDistance(5.0f);
    cam.Zoom(100.0f);
    REQUIRE_THAT(cam.Distance(), WithinAbs(0.5f, 1.0e-4f));

    cam.SetDistance(5.0f);
    cam.SetMode(leon::ECameraMode::FreeLook);
    cam.Zoom(2.0f);
    REQUIRE_THAT(cam.Distance(), WithinAbs(5.0f, 1.0e-4f));
}

TEST_CASE("Camera orbit position follows target and distance", "[core][camera]") {
    leon::Camera cam;
    cam.SetMode(leon::ECameraMode::Orbit);
    cam.SetTarget({0.0f, 0.0f, 0.0f});
    cam.SetYawPitch(0.0f, 0.0f);
    cam.SetDistance(4.0f);

    const glm::vec3 eye = cam.GetCameraLocation();
    REQUIRE_THAT(glm::length(eye), WithinAbs(4.0f, 1.0e-3f));
    REQUIRE_THAT(glm::length(cam.ForwardVector()), WithinAbs(1.0f, 1.0e-4f));
    REQUIRE_THAT(glm::length(cam.RightVector()), WithinAbs(1.0f, 1.0e-4f));
}

TEST_CASE("Camera FreeLook uses eye location", "[core][camera]") {
    leon::Camera cam;
    cam.SetMode(leon::ECameraMode::FreeLook);
    cam.SetEyeLocation({1.0f, 2.0f, 3.0f});
    const glm::vec3 eye = cam.GetCameraLocation();
    REQUIRE_THAT(eye.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(eye.y, WithinAbs(2.0f, 1.0e-5f));
    REQUIRE_THAT(eye.z, WithinAbs(3.0f, 1.0e-5f));
}
