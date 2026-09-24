#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Engine/Level.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include <memory>

using Catch::Matchers::WithinAbs;

TEST_CASE("SegmentTriangle hits a unit floor tri", "[physics][triangle]") {
    const glm::vec3 V0{-1.0f, 0.0f, -1.0f};
    const glm::vec3 V1{1.0f, 0.0f, -1.0f};
    const glm::vec3 V2{0.0f, 0.0f, 1.0f};
    float T = 1.0f;
    glm::vec3 N{};
    REQUIRE(SegmentTriangle({0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, V0, V1, V2, T, N));
    REQUIRE_THAT(T, WithinAbs(2.0f / 3.0f, 1.0e-3f));
    REQUIRE(N.y > 0.5f);
}

TEST_CASE("LineTrace and QuerySupportY use TriangleMesh surface", "[physics][triangle][trace]") {
    ULevel Level;
    FMeshData Data;
    // Flat plane at y=0.5 covering xz [-2,2]
    Data.Vertices.push_back({{-2.0f, 0.5f, -2.0f}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1}});
    Data.Vertices.push_back({{2.0f, 0.5f, -2.0f}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1}});
    Data.Vertices.push_back({{2.0f, 0.5f, 2.0f}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1}});
    Data.Vertices.push_back({{-2.0f, 0.5f, 2.0f}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1}});
    Data.Indices = {0, 1, 2, 0, 2, 3};
    Data.Submeshes.push_back({0, 6, 0});

    UStaticMeshComponent Component{};
    Component.mesh = std::make_shared<UStaticMesh>(UStaticMesh::CreateCpu(Data));
    Component.collisionEnabled = true;
    Level.StaticMeshes().push_back(std::move(Component));

    FPhysScene Scene;
    Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    Scene.SyncFromLevel(Level);
    REQUIRE(Scene.GetBodies()[0].CollisionShape == ECollisionShape::TriangleMesh);

    FHitResult Hit{};
    REQUIRE(Scene.LineTraceSingleByChannel(Hit, {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                                           ECollisionChannel::WorldStatic));
    REQUIRE(Hit.bBlockingHit);
    REQUIRE_THAT(Hit.ImpactPoint.y, WithinAbs(0.5f, 2.0e-2f));
    REQUIRE(Hit.ImpactNormal.y > 0.5f);

    FCapsuleShape Capsule{};
    Capsule.Radius = 0.35f;
    Capsule.Height = 1.0f;
    const float Support =
        Scene.QuerySupportY(Capsule, {0.0f, 1.0f, 0.0f}, 0.0f, 0.4f, 0.02f, ULevel::npos);
    REQUIRE_THAT(Support, WithinAbs(0.5f, 5.0e-2f));
}
