#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "Primitives.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("MakeCube has expected topology and bounds", "[render][primitives]") {
    const leon::MeshData cube = leon::MakeCube();
    REQUIRE_FALSE(cube.empty());
    REQUIRE(cube.vertices.size() == 24);
    REQUIRE(cube.indices.size() == 36);

    for (const auto& v : cube.vertices) {
        REQUIRE(std::abs(v.position.x) <= 0.5f + 1.0e-4f);
        REQUIRE(std::abs(v.position.y) <= 0.5f + 1.0e-4f);
        REQUIRE(std::abs(v.position.z) <= 0.5f + 1.0e-4f);
    }
}

TEST_CASE("MakePlane lies on XZ", "[render][primitives]") {
    const leon::MeshData plane = leon::MakePlane(2.0f);
    REQUIRE_FALSE(plane.empty());
    for (const auto& v : plane.vertices) {
        REQUIRE_THAT(v.position.y, WithinAbs(0.0f, 1.0e-5f));
    }
}

TEST_CASE("MakeSphere clamps low tessellation", "[render][primitives]") {
    const leon::MeshData sphere = leon::MakeSphere(2, 1);
    REQUIRE_FALSE(sphere.empty());
    REQUIRE(sphere.indices.size() % 3 == 0);
}
