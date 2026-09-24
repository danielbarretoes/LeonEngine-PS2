#include <catch2/catch_test_macros.hpp>
#include "Debug/DebugDraw.h"
#include "CollisionQuery.h"
#include "Physics/PhysScene.h"
#include <vector>

TEST_CASE("DrawDebugLineTrace miss and hit fill DebugDraw", "[physics][trace][debug]") {
    FDebugDraw Draw;

    DrawDebugLineTrace(Draw, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {});
    REQUIRE_FALSE(Draw.IsEmpty());

    Draw.Clear();
    FHitResult Hit{};
    Hit.bBlockingHit = true;
    Hit.Time = 0.5f;
    Hit.ImpactPoint = {0.0f, 0.5f, 0.0f};
    Hit.ImpactNormal = {0.0f, 1.0f, 0.0f};
    DrawDebugLineTrace(Draw, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {Hit});
    REQUIRE_FALSE(Draw.IsEmpty());
}

TEST_CASE("LineTrace ForOneFrame draws via PhysScene", "[physics][trace][debug]") {
    FPhysScene Scene;
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    Scene.GetBodies()[Id].Position = {0.0f, 0.5f, 0.0f};
    Scene.GetBodies()[Id].HalfExtents = {0.5f, 0.5f, 0.5f};

    FDebugDraw Draw;
    FCollisionQueryParams Params{};
    Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;

    FHitResult Hit{};
    REQUIRE(Scene.LineTraceSingleByChannel(Hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                           ECollisionChannel::WorldStatic, Params, &Draw));
    REQUIRE_FALSE(Draw.IsEmpty());

    Draw.Clear();
    std::vector<FHitResult> Misses;
    REQUIRE_FALSE(Scene.LineTraceMultiByChannel(Misses, {10.0f, 0.5f, -2.0f}, {10.0f, 0.5f, 2.0f},
                                                ECollisionChannel::WorldStatic, Params,
                                                &Draw));
    REQUIRE_FALSE(Draw.IsEmpty());
}

TEST_CASE("DrawDebugSphereTrace and CapsuleTrace fill batch", "[physics][trace][debug]") {
    FDebugDraw Draw;
    DrawDebugSphereTrace(Draw, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.35f, {});
    REQUIRE_FALSE(Draw.IsEmpty());

    Draw.Clear();
    DrawDebugCapsuleTrace(Draw, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.3f, 0.5f, {});
    REQUIRE_FALSE(Draw.IsEmpty());
}
