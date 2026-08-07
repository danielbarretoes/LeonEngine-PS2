#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <leon/level/Level.h>
#include <leon/physics/PhysScene.h>
#include <leon/physics/TriangleCollision.h>
#include <leon/render/MeshData.h>
#include <leon/render/StaticMesh.h>
#include <memory>

using Catch::Matchers::WithinAbs;

TEST_CASE("SegmentTriangle hits a unit floor tri", "[physics][triangle]") {
    const glm::vec3 v0{-1.0f, 0.0f, -1.0f};
    const glm::vec3 v1{1.0f, 0.0f, -1.0f};
    const glm::vec3 v2{0.0f, 0.0f, 1.0f};
    float t = 1.0f;
    glm::vec3 n{};
    REQUIRE(leon::SegmentTriangle({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, v0, v1, v2, t, n));
    REQUIRE_THAT(t, WithinAbs(2.0f / 3.0f, 1.0e-3f));
    REQUIRE(n.y > 0.5f);
}

TEST_CASE("LineTrace and QuerySupportY use TriangleMesh surface", "[physics][triangle][trace]") {
    leon::Level level;
    leon::MeshData data;
    // Flat plane at y=0.5 covering xz [-2,2]
    data.vertices.push_back({{-2.0f, 0.5f, -2.0f}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1}});
    data.vertices.push_back({{2.0f, 0.5f, -2.0f}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1}});
    data.vertices.push_back({{2.0f, 0.5f, 2.0f}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1}});
    data.vertices.push_back({{-2.0f, 0.5f, 2.0f}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1}});
    data.indices = {0, 1, 2, 0, 2, 3};
    data.submeshes.push_back({0, 6, 0});

    leon::StaticMeshComponent component{};
    component.mesh = std::make_shared<leon::StaticMesh>(leon::StaticMesh::CreateCpu(data));
    component.collisionEnabled = true;
    level.StaticMeshes().push_back(std::move(component));

    leon::PhysScene scene;
    scene.AddBody({0, leon::EBodyType::Static, 1.0f, true});
    scene.SyncFromLevel(level);
    REQUIRE(scene.Bodies()[0].collisionShape == leon::ECollisionShape::TriangleMesh);

    leon::HitResult hit{};
    REQUIRE(scene.LineTraceSingleByChannel(hit, {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                                           leon::ECollisionChannel::WorldStatic));
    REQUIRE(hit.bBlockingHit);
    REQUIRE_THAT(hit.ImpactPoint.y, WithinAbs(0.5f, 2.0e-2f));
    REQUIRE(hit.ImpactNormal.y > 0.5f);

    leon::CapsuleShape capsule{};
    capsule.radius = 0.35f;
    capsule.height = 1.0f;
    const float support =
        scene.QuerySupportY(capsule, {0.0f, 1.0f, 0.0f}, 0.0f, 0.4f, 0.02f, leon::Level::npos);
    REQUIRE_THAT(support, WithinAbs(0.5f, 5.0e-2f));
}
