#include <catch2/catch_test_macros.hpp>
#include "Debug/DebugDraw.h"
#include "CollisionQuery.h"
#include "Physics/PhysScene.h"
#include <vector>

TEST_CASE("DrawDebugLineTrace miss and hit fill DebugDraw", "[physics][trace][debug]") {
    FDebugDraw draw;

    DrawDebugLineTrace(draw, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {});
    REQUIRE_FALSE(draw.IsEmpty());

    draw.Clear();
    FHitResult hit{};
    hit.bBlockingHit = true;
    hit.Time = 0.5f;
    hit.ImpactPoint = {0.0f, 0.5f, 0.0f};
    hit.ImpactNormal = {0.0f, 1.0f, 0.0f};
    DrawDebugLineTrace(draw, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {hit});
    REQUIRE_FALSE(draw.IsEmpty());
}

TEST_CASE("LineTrace ForOneFrame draws via PhysScene", "[physics][trace][debug]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    scene.GetBodies()[id].Position = {0.0f, 0.5f, 0.0f};
    scene.GetBodies()[id].HalfExtents = {0.5f, 0.5f, 0.5f};

    FDebugDraw draw;
    FCollisionQueryParams params{};
    params.DrawDebugType = EDrawDebugTrace::ForOneFrame;

    FHitResult hit{};
    REQUIRE(scene.LineTraceSingleByChannel(hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                           ECollisionChannel::WorldStatic, params, &draw));
    REQUIRE_FALSE(draw.IsEmpty());

    draw.Clear();
    std::vector<FHitResult> misses;
    REQUIRE_FALSE(scene.LineTraceMultiByChannel(misses, {10.0f, 0.5f, -2.0f}, {10.0f, 0.5f, 2.0f},
                                                ECollisionChannel::WorldStatic, params,
                                                &draw));
    REQUIRE_FALSE(draw.IsEmpty());
}

TEST_CASE("DrawDebugSphereTrace and CapsuleTrace fill batch", "[physics][trace][debug]") {
    FDebugDraw draw;
    DrawDebugSphereTrace(draw, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.35f, {});
    REQUIRE_FALSE(draw.IsEmpty());

    draw.Clear();
    DrawDebugCapsuleTrace(draw, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.3f, 0.5f, {});
    REQUIRE_FALSE(draw.IsEmpty());
}
