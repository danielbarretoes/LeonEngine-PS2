#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <leon/physics/PhysScene.h>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("LineTraceSingleByChannel hits static AABB", "[physics][trace]") {
    leon::PhysScene scene;
    const std::size_t id = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[id].position = {0.0f, 0.5f, 0.0f};
    scene.Bodies()[id].halfExtents = {0.5f, 0.5f, 0.5f};

    leon::HitResult hit{};
    REQUIRE(scene.LineTraceSingleByChannel(hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                           leon::ECollisionChannel::WorldStatic));
    REQUIRE(hit.bBlockingHit);
    REQUIRE(hit.Time < 1.0f);
    REQUIRE_THAT(hit.ImpactNormal.z, WithinAbs(-1.0f, 1.0e-3f));
}

TEST_CASE("LineTraceSingleByChannel filters by channel", "[physics][trace]") {
    leon::PhysScene scene;
    const std::size_t id = scene.AddBody({0, leon::EBodyType::Dynamic, 1.0f, true});
    scene.Bodies()[id].position = {0.0f, 0.5f, 0.0f};
    scene.Bodies()[id].halfExtents = {0.5f, 0.5f, 0.5f};

    leon::HitResult hit{};
    REQUIRE_FALSE(scene.LineTraceSingleByChannel(hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                                 leon::ECollisionChannel::WorldStatic));
    REQUIRE(scene.LineTraceSingleByChannel(hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                           leon::ECollisionChannel::WorldDynamic));
}

TEST_CASE("SphereTraceSingleByChannel hits floor plane", "[physics][trace]") {
    leon::PhysScene scene;
    leon::CollisionQueryParams params{};
    params.bTraceFloorPlane = true;
    params.FloorY = 0.0f;

    leon::HitResult hit{};
    REQUIRE(scene.SphereTraceSingleByChannel(hit, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.35f,
                                             leon::ECollisionChannel::Visibility, params));
    REQUIRE(hit.bFloorPlane);
    REQUIRE_THAT(hit.ImpactPoint.y, WithinAbs(0.0f, 1.0e-3f));
    REQUIRE(hit.ImpactNormal.y > 0.5f);
}

TEST_CASE("CapsuleTraceSingleByChannel finds platform top", "[physics][trace]") {
    leon::PhysScene scene;
    const std::size_t id = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[id].position = {0.0f, 1.0f, 0.0f};
    scene.Bodies()[id].halfExtents = {1.0f, 1.0f, 1.0f}; // top at y=2

    leon::HitResult hit{};
    const float radius = 0.35f;
    const float halfHeight = 0.5f;
    REQUIRE(scene.CapsuleTraceSingleByChannel(hit, {0.0f, 3.0f, 0.0f}, {0.0f, 1.5f, 0.0f}, radius,
                                              halfHeight, leon::ECollisionChannel::Visibility));
    REQUIRE(hit.bBlockingHit);
    REQUIRE(hit.ImpactPoint.y <= 2.0f + 1.0e-2f);
    REQUIRE(hit.ImpactNormal.y > 0.5f);
}

TEST_CASE("LineTraceMultiByChannel returns all hits sorted", "[physics][trace][multi]") {
    leon::PhysScene scene;
    const std::size_t nearId = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[nearId].position = {0.0f, 0.5f, 0.0f};
    scene.Bodies()[nearId].halfExtents = {0.5f, 0.5f, 0.5f};

    const std::size_t farId = scene.AddBody({1, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[farId].position = {0.0f, 0.5f, 3.0f};
    scene.Bodies()[farId].halfExtents = {0.5f, 0.5f, 0.5f};

    leon::CollisionQueryParams params{};
    params.bTraceFloorPlane = false;

    std::vector<leon::HitResult> hits;
    REQUIRE(scene.LineTraceMultiByChannel(hits, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 5.0f},
                                          leon::ECollisionChannel::WorldStatic, params));
    REQUIRE(hits.size() == 2);
    REQUIRE(hits[0].Time < hits[1].Time);
    REQUIRE(hits[0].LevelMeshIndex == 0);
    REQUIRE(hits[1].LevelMeshIndex == 1);

    leon::HitResult single{};
    REQUIRE(scene.LineTraceSingleByChannel(single, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 5.0f},
                                           leon::ECollisionChannel::WorldStatic, params));
    REQUIRE_THAT(single.Time, WithinAbs(hits[0].Time, 1.0e-5f));
}

TEST_CASE("SphereTraceMultiByChannel includes floor and bodies", "[physics][trace][multi]") {
    leon::PhysScene scene;
    const std::size_t id = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[id].position = {0.0f, 2.0f, 0.0f};
    scene.Bodies()[id].halfExtents = {1.0f, 0.5f, 1.0f}; // top at 2.5, bottom at 1.5

    leon::CollisionQueryParams params{};
    params.bTraceFloorPlane = true;
    params.FloorY = 0.0f;

    std::vector<leon::HitResult> hits;
    REQUIRE(scene.SphereTraceMultiByChannel(hits, {0.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.25f,
                                            leon::ECollisionChannel::Visibility, params));
    REQUIRE(hits.size() >= 2);
    REQUIRE(hits.front().Time <= hits.back().Time);
    bool anyFloor = false;
    bool anyBody = false;
    for (const leon::HitResult& h : hits) {
        anyFloor = anyFloor || h.bFloorPlane;
        anyBody = anyBody || !h.bFloorPlane;
    }
    REQUIRE(anyFloor);
    REQUIRE(anyBody);
}

TEST_CASE("CapsuleTraceMultiByChannel returns multiple blocking hits", "[physics][trace][multi]") {
    leon::PhysScene scene;
    const std::size_t a = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.Bodies()[a].position = {0.0f, 1.0f, 0.0f};
    scene.Bodies()[a].halfExtents = {0.5f, 0.5f, 0.5f};
    const std::size_t b = scene.AddBody({1, leon::EBodyType::Dynamic, 1.0f, true});
    scene.Bodies()[b].position = {0.0f, 3.0f, 0.0f};
    scene.Bodies()[b].halfExtents = {0.5f, 0.5f, 0.5f};

    std::vector<leon::HitResult> hits;
    REQUIRE(scene.CapsuleTraceMultiByChannel(hits, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.2f,
                                             0.3f, leon::ECollisionChannel::Visibility));
    REQUIRE(hits.size() == 2);
    REQUIRE(hits[0].Time < hits[1].Time);
}
