#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "Physics/PhysScene.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("PhysScene AddBody and Clear", "[physics][physscene]") {
    FPhysScene Scene;
    REQUIRE(Scene.GetBodies().empty());
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    REQUIRE(Id == 0);
    REQUIRE(Scene.GetBodies().size() == 1);
    Scene.Clear();
    REQUIRE(Scene.GetBodies().empty());
}

TEST_CASE("PhysScene Step applies gravity and snaps to floor", "[physics][physscene]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({0, EBodyType::Dynamic, 1.0f, true});
    auto& Body = Scene.GetBodies()[Id];
    Body.Position = {0.0f, 2.0f, 0.0f};
    Body.HalfExtents = {0.5f, 0.5f, 0.5f};

    FPhysSceneStepParams Params;
    Params.DeltaTime = 1.0f / 60.0f;
    Params.Gravity = 24.0f;
    Params.FloorY = 0.0f;
    Params.Skin = 0.02f;

    for (int I = 0; I < 180; ++I) {
        Scene.Step(Params);
    }

    // Bottom of AABB ≈ floorY (center y ≈ halfExtents.y)
    REQUIRE(Body.Position.y < 1.0f);
    REQUIRE(Body.Position.y > 0.0f);
    REQUIRE_THAT(Body.VelocityY, WithinAbs(0.0f, 0.5f));
}

TEST_CASE("PhysScene static body does not move under gravity", "[physics][physscene]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& Body = Scene.GetBodies()[Id];
    Body.Position = {1.0f, 3.0f, 2.0f};
    Body.HalfExtents = {0.5f, 0.5f, 0.5f};

    FPhysSceneStepParams Params;
    Params.DeltaTime = 0.1f;
    Params.Gravity = 50.0f;
    Scene.Step(Params);

    REQUIRE_THAT(Body.Position.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(Body.Position.y, WithinAbs(3.0f, 1.0e-5f));
    REQUIRE_THAT(Body.Position.z, WithinAbs(2.0f, 1.0e-5f));
}

TEST_CASE("PhysScene QuerySupportY uses body tops", "[physics][physscene]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& Body = Scene.GetBodies()[Id];
    Body.Position = {0.0f, 1.0f, 0.0f};
    Body.HalfExtents = {1.0f, 1.0f, 1.0f}; // top at y=2

    FCapsuleShape Capsule;
    Capsule.Radius = 0.35f;
    Capsule.Height = 1.85f;

    const float Support =
        Scene.QuerySupportY(Capsule, {0.0f, 2.1f, 0.0f}, 0.0f, 0.35f, 0.02f, ULevel::Npos);
    REQUIRE_THAT(Support, WithinAbs(2.0f, 1.0e-3f));
}

TEST_CASE("PhysScene ResolveCapsuleSides pushes out of AABB", "[physics][physscene]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    auto& Body = Scene.GetBodies()[Id];
    Body.Position = {0.0f, 0.9f, 0.0f};
    Body.HalfExtents = {0.5f, 0.9f, 0.5f};

    FCapsuleShape Capsule;
    Capsule.Radius = 0.35f;
    Capsule.Height = 1.85f;

    glm::vec3 Feet{0.1f, 0.0f, 0.0f};
    FCapsuleContactParams Contact{};
    Scene.ResolveCapsuleSides(Capsule, Feet, {0.0f, 0.0f}, Contact, ULevel::Npos, false);

    const float DistXz = std::sqrt((Feet.x * Feet.x) + (Feet.z * Feet.z));
    REQUIRE(DistXz > 0.4f);
}

TEST_CASE("PhysScene ApplyCapsuleSweepPush moves Dynamic without penetration",
          "[physics][physscene][push]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({7, EBodyType::Dynamic, 1.0f, true});
    FBodyInstance& Body = Scene.GetBodies()[Id];
    Body.Position = {2.0f, 0.5f, 0.0f};
    Body.HalfExtents = {0.5f, 0.5f, 0.5f};
    Body.Mass = 1.0f;

    REQUIRE(Scene.ApplyCapsuleSweepPush(7, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 0.85f, 18.0f));
    REQUIRE(Body.VelXz.x > 0.1f);
    REQUIRE(Body.Position.x > 2.0f);

    REQUIRE_FALSE(Scene.ApplyCapsuleSweepPush(7, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 0.85f, 18.0f));
}
