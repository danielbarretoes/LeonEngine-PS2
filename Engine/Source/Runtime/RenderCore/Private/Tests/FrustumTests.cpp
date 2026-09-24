#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Frustum.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Aabb fromLocalTransformed expands under rotation", "[render][frustum]") {
    const glm::mat4 Model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
    const FBox Box =
        FBox::FromLocalTransformed({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, Model);
    REQUIRE(Box.Min.x < -0.5f);
    REQUIRE(Box.Max.x > 0.5f);
    REQUIRE(Box.Min.y <= -0.5f + 1.0e-4f);
    REQUIRE(Box.Max.y >= 0.5f - 1.0e-4f);
}

TEST_CASE("Aabb intersectRay hits unit cube from -Z", "[render][frustum]") {
    const FBox Box{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    float T = -1.0f;
    REQUIRE(Box.IntersectRay({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, T));
    REQUIRE_THAT(T, WithinAbs(4.5f, 1.0e-4f));

    REQUIRE_FALSE(Box.IntersectRay({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 1.0f}, T));
    REQUIRE_FALSE(Box.IntersectRay({2.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, T));
}

TEST_CASE("Frustum intersectsAabb contains near origin box", "[render][frustum]") {
    const glm::mat4 View =
        glm::lookAt(glm::vec3{0.0f, 0.0f, 5.0f}, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 Proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    FFrustum Frustum;
    Frustum.ExtractFromViewProjection(Proj * View);

    FBox Inside{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    REQUIRE(Frustum.IntersectsAabb(Inside));

    FBox FarAway{{200.0f, 200.0f, 200.0f}, {201.0f, 201.0f, 201.0f}};
    REQUIRE_FALSE(Frustum.IntersectsAabb(FarAway));
}
