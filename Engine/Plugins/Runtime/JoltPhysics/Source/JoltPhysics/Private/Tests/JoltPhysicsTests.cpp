#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <leon/gameplay/World.h>
#include <leon/level/Level.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/MeshData.h>
#include <leon/render/StaticMesh.h>
#include <memory>
#include <string>

using Catch::Matchers::WithinAbs;

#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT

TEST_CASE("PhysScene Jolt backend reports Jolt", "[physics][jolt]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);
    REQUIRE(scene.GetBackend() == leon::EPhysicsBackend::Jolt);
    REQUIRE(scene.GetBackendIface() != nullptr);
    REQUIRE(std::string(scene.GetBackendIface()->GetName()) == "Jolt");
    REQUIRE(scene.GetBackendIface()->HasRigidWorld());
}

TEST_CASE("World SetPhysicsBackend switches to Jolt", "[physics][jolt][world]") {
    leon::World world;
    REQUIRE(world.GetPhysicsScene().GetBackend() == leon::EPhysicsBackend::Arcade);

    world.SetPhysicsBackend(leon::EPhysicsBackend::Jolt);
    REQUIRE(world.GetPhysicsScene().GetBackend() == leon::EPhysicsBackend::Jolt);

    const std::size_t id = world.GetPhysicsScene().AddBody({0, leon::EBodyType::Dynamic, 8.0f, true});
    auto& body = world.GetPhysicsScene().Bodies()[id];
    body.position = {0.0f, 2.5f, 0.0f};
    body.halfExtents = {0.4f, 0.4f, 0.4f};

    leon::PhysSceneStepParams params;
    params.deltaTime = 1.0f / 60.0f;
    params.gravity = 24.0f;
    params.floorY = 0.0f;
    params.walkBounds = 50.0f;
    for (int i = 0; i < 180; ++i) {
        world.GetPhysicsScene().Step(params);
    }
    REQUIRE(body.position.y < 1.1f);
    REQUIRE(body.position.y > 0.2f);
}

TEST_CASE("PhysScene Jolt Step applies gravity and rests on floor", "[physics][jolt]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);
    const std::size_t id = scene.AddBody({0, leon::EBodyType::Dynamic, 10.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 3.0f, 0.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};

    leon::PhysSceneStepParams params;
    params.deltaTime = 1.0f / 60.0f;
    params.gravity = 24.0f;
    params.floorY = 0.0f;
    params.walkBounds = 100.0f;

    for (int i = 0; i < 240; ++i) {
        scene.Step(params);
    }

    REQUIRE(body.position.y < 1.2f);
    REQUIRE(body.position.y > 0.3f);
    REQUIRE(std::abs(body.velocityY) < 1.0f);
}

TEST_CASE("PhysScene Jolt dynamic rests on static box", "[physics][jolt]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);

    const std::size_t groundId = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    auto& ground = scene.Bodies()[groundId];
    ground.position = {0.0f, 0.5f, 0.0f};
    ground.halfExtents = {2.0f, 0.5f, 2.0f};

    const std::size_t boxId = scene.AddBody({1, leon::EBodyType::Dynamic, 5.0f, true});
    auto& box = scene.Bodies()[boxId];
    box.position = {0.0f, 4.0f, 0.0f};
    box.halfExtents = {0.4f, 0.4f, 0.4f};

    leon::PhysSceneStepParams params;
    params.deltaTime = 1.0f / 60.0f;
    params.gravity = 24.0f;
    params.floorY = -10.0f; // below ground so static box is the support
    params.walkBounds = 100.0f;

    for (int i = 0; i < 300; ++i) {
        scene.Step(params);
    }

    // Dynamic COM should settle near ground top (1.0) + halfExtents (0.4) ≈ 1.4
    REQUIRE_THAT(box.position.y, WithinAbs(1.4f, 0.35f));
    REQUIRE(std::abs(box.velocityY) < 1.5f);
}

TEST_CASE("PhysScene Jolt dynamic rests on TriangleMesh static", "[physics][jolt][mesh]") {
    leon::Level level;
    leon::MeshData data;
    // Flat plane at y=1 covering xz [-3,3]
    data.vertices.push_back({{-3.0f, 1.0f, -3.0f}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1}});
    data.vertices.push_back({{3.0f, 1.0f, -3.0f}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1}});
    data.vertices.push_back({{3.0f, 1.0f, 3.0f}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1}});
    data.vertices.push_back({{-3.0f, 1.0f, 3.0f}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1}});
    data.indices = {0, 1, 2, 0, 2, 3};
    data.submeshes.push_back({0, 6, 0});

    leon::StaticMeshComponent component{};
    component.mesh = std::make_shared<leon::StaticMesh>(leon::StaticMesh::CreateCpu(data));
    component.collisionEnabled = true;
    level.StaticMeshes().push_back(std::move(component));

    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);
    scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    const std::size_t boxId = scene.AddBody({1, leon::EBodyType::Dynamic, 5.0f, true});
    auto& box = scene.Bodies()[boxId];
    box.position = {0.0f, 5.0f, 0.0f};
    box.halfExtents = {0.35f, 0.35f, 0.35f};

    scene.SyncFromLevel(level);
    REQUIRE(scene.Bodies()[0].collisionShape == leon::ECollisionShape::TriangleMesh);

    leon::PhysSceneStepParams params;
    params.deltaTime = 1.0f / 60.0f;
    params.gravity = 24.0f;
    params.floorY = -20.0f;
    params.walkBounds = 100.0f;

    for (int i = 0; i < 360; ++i) {
        scene.Step(params);
    }

    // Plane at y=1 + halfExtents 0.35 ≈ 1.35
    REQUIRE_THAT(box.position.y, WithinAbs(1.35f, 0.45f));
    REQUIRE(std::abs(box.velocityY) < 2.0f);
}

TEST_CASE("PhysScene Jolt LineTrace hits static box", "[physics][jolt][trace]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);
    REQUIRE(scene.GetBackendIface()->HasNarrowPhaseTraces());

    const std::size_t id = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 0.5f, 0.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};

    leon::CollisionQueryParams params;
    params.bTraceFloorPlane = false;
    leon::HitResult hit{};
    REQUIRE(scene.LineTraceSingleByChannel(hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f},
                                           leon::ECollisionChannel::WorldStatic, params));
    REQUIRE(hit.bBlockingHit);
    REQUIRE_THAT(hit.ImpactPoint.z, WithinAbs(-0.5f, 0.08f));
    REQUIRE(hit.ImpactNormal.z < -0.5f);
}

TEST_CASE("PhysScene Jolt SphereTrace hits static box", "[physics][jolt][trace]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);

    const std::size_t id = scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    auto& body = scene.Bodies()[id];
    body.position = {0.0f, 0.5f, 0.0f};
    body.halfExtents = {0.5f, 0.5f, 0.5f};

    leon::CollisionQueryParams params;
    params.bTraceFloorPlane = false;
    leon::HitResult hit{};
    REQUIRE(scene.SphereTraceSingleByChannel(hit, {0.0f, 0.5f, -3.0f}, {0.0f, 0.5f, 3.0f}, 0.25f,
                                             leon::ECollisionChannel::WorldStatic, params));
    REQUIRE(hit.bBlockingHit);
    // Sweep center stops before the face by ~radius.
    REQUIRE_THAT(hit.Location.z, WithinAbs(-0.75f, 0.12f));
}

#else

TEST_CASE("PhysScene Jolt disabled falls back to Arcade", "[physics][jolt]") {
    leon::PhysScene scene(leon::EPhysicsBackend::Jolt);
    REQUIRE(scene.GetBackend() == leon::EPhysicsBackend::Arcade);
}

#endif
