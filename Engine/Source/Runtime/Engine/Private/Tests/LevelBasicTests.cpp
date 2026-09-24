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
    const glm::vec3 dir = leon::lightDirectionFromRotation(rot);
    REQUIRE_THAT(glm::length(dir), WithinAbs(1.0f, 1.0e-4f));
    const glm::vec3 back = leon::rotationFromLightDirection(dir);
    REQUIRE_THAT(back.x, WithinAbs(rot.x, 1.0e-2f));
    REQUIRE_THAT(back.y, WithinAbs(rot.y, 1.0e-2f));
}

TEST_CASE("DirectionalLight GetDirection matches transform", "[level][light]") {
    leon::DirectionalLight light;
    light.transform.rotationDegrees = {30.0f, 0.0f, 0.0f};
    const glm::vec3 dir = light.GetDirection();
    REQUIRE(dir.y < 0.0f);
}

TEST_CASE("tryParseBasicShapeName is case-insensitive", "[level][basicshape]") {
    leon::EBasicShape shape{};
    REQUIRE(leon::tryParseBasicShapeName("Cube", shape));
    REQUIRE(shape == leon::EBasicShape::Cube);
    REQUIRE(leon::tryParseBasicShapeName("sphere", shape));
    REQUIRE(shape == leon::EBasicShape::Sphere);
    REQUIRE(leon::tryParseBasicShapeName("PLANE", shape));
    REQUIRE(shape == leon::EBasicShape::Plane);
    REQUIRE_FALSE(leon::tryParseBasicShapeName("Octahedron", shape));
}

TEST_CASE("BlockingVolume and PlayerStart name helpers", "[level][basicshape]") {
    REQUIRE(leon::isBlockingVolumeName("BlockingVolume"));
    REQUIRE(leon::isBlockingVolumeName("blockingvolume"));
    REQUIRE(leon::isPlayerStartName("PlayerStart"));
    REQUIRE_FALSE(leon::isPlayerStartName("Cube"));
}

TEST_CASE("BasicShape factories set type and plane scale", "[level][basicshape]") {
    const leon::BasicShape cube = leon::BasicShape::cube();
    REQUIRE(cube.type == leon::EBasicShape::Cube);
    const leon::BasicShape plane = leon::BasicShape::plane(4.0f);
    REQUIRE(plane.type == leon::EBasicShape::Plane);
    REQUIRE_THAT(plane.transform.scale.x, WithinAbs(4.0f, 1.0e-5f));
    REQUIRE_THAT(plane.transform.scale.z, WithinAbs(4.0f, 1.0e-5f));
}

TEST_CASE("BasicLight parse and addTo Level", "[level][basiclight]") {
    leon::EBasicLight type{};
    REQUIRE(leon::tryParseBasicLightName("DirectionalLight", type));
    REQUIRE(type == leon::EBasicLight::Directional);
    REQUIRE(leon::tryParseBasicLightName("PointLight", type));
    REQUIRE(type == leon::EBasicLight::Point);

    leon::Level level;
    level.ClearLights();
    REQUIRE(level.DirectionalLights().empty());

    leon::BasicLight::directional().addTo(level);
    leon::BasicLight::point().addTo(level);
    REQUIRE(level.DirectionalLights().size() == 1);
    REQUIRE(level.PointLights().size() == 1);
}
