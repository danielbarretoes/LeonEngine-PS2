#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Engine/Level.h"
#include "Level/Light.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("lightDirectionFromRotation round-trip", "[level][light]") {
    const glm::vec3 rot{45.0f, 90.0f, 0.0f};
    const glm::vec3 dir = lightDirectionFromRotation(rot);
    REQUIRE_THAT(glm::length(dir), WithinAbs(1.0f, 1.0e-4f));
    const glm::vec3 back = rotationFromLightDirection(dir);
    REQUIRE_THAT(back.x, WithinAbs(rot.x, 1.0e-2f));
    REQUIRE_THAT(back.y, WithinAbs(rot.y, 1.0e-2f));
}

TEST_CASE("DirectionalLight GetDirection matches transform", "[level][light]") {
    FDirectionalLight light;
    light.transform.RotationDegrees = {30.0f, 0.0f, 0.0f};
    const glm::vec3 dir = light.GetDirection();
    REQUIRE(dir.y < 0.0f);
}

TEST_CASE("tryParseBasicShapeName is case-insensitive", "[level][basicshape]") {
    EBasicShape shape{};
    REQUIRE(tryParseBasicShapeName("Cube", shape));
    REQUIRE(shape == EBasicShape::Cube);
    REQUIRE(tryParseBasicShapeName("sphere", shape));
    REQUIRE(shape == EBasicShape::Sphere);
    REQUIRE(tryParseBasicShapeName("PLANE", shape));
    REQUIRE(shape == EBasicShape::Plane);
    REQUIRE_FALSE(tryParseBasicShapeName("Octahedron", shape));
}

TEST_CASE("BlockingVolume and PlayerStart name helpers", "[level][basicshape]") {
    REQUIRE(isBlockingVolumeName("BlockingVolume"));
    REQUIRE(isBlockingVolumeName("blockingvolume"));
    REQUIRE(isPlayerStartName("PlayerStart"));
    REQUIRE_FALSE(isPlayerStartName("Cube"));
}

TEST_CASE("BasicShape factories set type and plane scale", "[level][basicshape]") {
    const FBasicShape cube = FBasicShape::cube();
    REQUIRE(cube.type == EBasicShape::Cube);
    const FBasicShape plane = FBasicShape::plane(4.0f);
    REQUIRE(plane.type == EBasicShape::Plane);
    REQUIRE_THAT(plane.transform.Scale.x, WithinAbs(4.0f, 1.0e-5f));
    REQUIRE_THAT(plane.transform.Scale.z, WithinAbs(4.0f, 1.0e-5f));
}

TEST_CASE("BasicLight parse and addTo Level", "[level][basiclight]") {
    EBasicLight type{};
    REQUIRE(tryParseBasicLightName("DirectionalLight", type));
    REQUIRE(type == EBasicLight::Directional);
    REQUIRE(tryParseBasicLightName("PointLight", type));
    REQUIRE(type == EBasicLight::Point);

    ULevel level;
    level.ClearLights();
    REQUIRE(level.DirectionalLights().empty());

    FBasicLight::directional().addTo(level);
    FBasicLight::point().addTo(level);
    REQUIRE(level.DirectionalLights().size() == 1);
    REQUIRE(level.PointLights().size() == 1);
}
