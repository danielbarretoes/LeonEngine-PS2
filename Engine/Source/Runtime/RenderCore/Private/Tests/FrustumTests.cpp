#include <glm/gtc/matrix_transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Frustum.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Aabb fromLocalTransformed expands under rotation", "[render][frustum]") {
    const glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
    const Aabb box =
        Aabb::fromLocalTransformed({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, model);
    REQUIRE(box.min.x < -0.5f);
    REQUIRE(box.max.x > 0.5f);
    REQUIRE(box.min.y <= -0.5f + 1.0e-4f);
    REQUIRE(box.max.y >= 0.5f - 1.0e-4f);
}

TEST_CASE("Aabb intersectRay hits unit cube from -Z", "[render][frustum]") {
    const Aabb box{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    float t = -1.0f;
    REQUIRE(box.intersectRay({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, t));
    REQUIRE_THAT(t, WithinAbs(4.5f, 1.0e-4f));

    REQUIRE_FALSE(box.intersectRay({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 1.0f}, t));
    REQUIRE_FALSE(box.intersectRay({2.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, t));
}

TEST_CASE("Frustum intersectsAabb contains near origin box", "[render][frustum]") {
    const glm::mat4 view =
        glm::lookAt(glm::vec3{0.0f, 0.0f, 5.0f}, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    Frustum frustum;
    frustum.extractFromViewProjection(proj * view);

    Aabb inside{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    REQUIRE(frustum.intersectsAabb(inside));

    Aabb farAway{{200.0f, 200.0f, 200.0f}, {201.0f, 201.0f, 201.0f}};
    REQUIRE_FALSE(frustum.intersectsAabb(farAway));
}
