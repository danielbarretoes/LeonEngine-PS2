#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "Physics/PhysScene.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("PhysScene AddBody and Clear", "[physics][physscene]") {
    FPhysScene scene;
    REQUIRE(scene.Bodies().empty());
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    REQUIRE(id == 0);
    REQUIRE(scene.Bodies().size() == 1);
    scene.Clear();
    REQUIRE(scene.Bodies().empty());
}

TEST_CASE("PhysScene Step applies gravity and snaps to floor", "[physics][physscene]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Dynamic, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 2.0f, 0.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};

    FPhysSceneStepParams params;
    params.deltaTime = 1.0f / 60.0f;
    params.gravity = 24.0f;
    params.floorY = 0.0f;
    params.skin = 0.02f;

    for (int i = 0; i < 180; ++i) {
        scene.Step(params);
    }

    // Bottom of AABB ≈ floorY (center y ≈ halfExtents.y)
    REQUIRE(body.position.y < 1.0f);
    REQUIRE(body.position.y > 0.0f);
    REQUIRE_THAT(body.velocityY, WithinAbs(0.0f, 0.5f));
}

TEST_CASE("PhysScene static body does not move under gravity", "[physics][physscene]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {1.0f, 3.0f, 2.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};

    FPhysSceneStepParams params;
    params.deltaTime = 0.1f;
    params.gravity = 50.0f;
    scene.Step(params);

    REQUIRE_THAT(body.position.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(body.position.y, WithinAbs(3.0f, 1.0e-5f));
    REQUIRE_THAT(body.position.z, WithinAbs(2.0f, 1.0e-5f));
}

TEST_CASE("PhysScene QuerySupportY uses body tops", "[physics][physscene]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 1.0f, 0.0f};
    body.halfExtents = {1.0f, 1.0f, 1.0f}; // top at y=2

    FCapsuleShape capsule;
    capsule.radius = 0.35f;
    capsule.height = 1.85f;

    const float support =
        scene.QuerySupportY(capsule, {0.0f, 2.1f, 0.0f}, 0.0f, 0.35f, 0.02f, Level::npos);
    REQUIRE_THAT(support, WithinAbs(2.0f, 1.0e-3f));
}

TEST_CASE("PhysScene ResolveCapsuleSides pushes out of AABB", "[physics][physscene]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 0.9f, 0.0f};
    body.halfExtents = {0.5f, 0.9f, 0.5f};

    FCapsuleShape capsule;
    capsule.radius = 0.35f;
    capsule.height = 1.85f;

    glm::vec3 feet{0.1f, 0.0f, 0.0f};
    FCapsuleContactParams contact{};
    scene.ResolveCapsuleSides(capsule, feet, {0.0f, 0.0f}, contact, Level::npos, false);

    const float distXZ = std::sqrt((feet.x * feet.x) + (feet.z * feet.z));
    REQUIRE(distXZ > 0.4f);
}

TEST_CASE("PhysScene ApplyCapsuleSweepPush moves Dynamic without penetration",
          "[physics][physscene][push]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({7, EBodyType::Dynamic, 1.0f, true});
    FBodyInstance& body = scene.Bodies()[id];
    body.position = {2.0f, 0.5f, 0.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};
    body.mass = 1.0f;

    REQUIRE(scene.ApplyCapsuleSweepPush(7, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 0.85f, 18.0f));
    REQUIRE(body.velXZ.x > 0.1f);
    REQUIRE(body.position.x > 2.0f);

    REQUIRE_FALSE(scene.ApplyCapsuleSweepPush(7, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 0.85f, 18.0f));
}
