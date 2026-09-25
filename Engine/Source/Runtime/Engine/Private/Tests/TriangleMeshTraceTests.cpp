#include "Engine/Level.h"
#include "MeshData.h"
#include "Physics/PhysScene.h"
#include "StaticMesh.h"
#include "TriangleCollision.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;

TEST_CASE("LineTrace and QuerySupportY use TriangleMesh surface", "[physics][triangle][trace]")
{
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
	Component.Mesh = std::make_shared<UStaticMesh>(UStaticMesh::CreateCpu(Data));
	Component.bCollisionEnabled = true;
	Level.GetStaticMeshes().push_back(std::move(Component));

	FPhysScene Scene;
	Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.SyncFromLevel(Level);
	REQUIRE(Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FHitResult Hit{};
	REQUIRE(
		Scene.LineTraceSingleByChannel(Hit, {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, ECollisionChannel::WorldStatic));
	REQUIRE(Hit.bBlockingHit);
	REQUIRE_THAT(Hit.ImpactPoint.Y, WithinAbs(0.5f, 2.0e-2f));
	REQUIRE(Hit.ImpactNormal.Y > 0.5f);

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(0.35f, 0.5f);
	const float Support = Scene.QuerySupportY(Capsule, {0.0f, 1.0f, 0.0f}, 0.0f, 0.4f, 0.02f, ULevel::Npos);
	REQUIRE_THAT(Support, WithinAbs(0.5f, 5.0e-2f));
}
