#include "Engine/Level.h"
#include "Engine/World.h"
#include "MeshData.h"
#include "Physics/PhysScene.h"
#include "StaticMesh.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <string>

using Catch::Matchers::WithinAbs;

#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT

TEST_CASE("PhysScene Jolt backend reports Jolt", "[physics][jolt]")
{
	FPhysScene Scene(EPhysicsBackend::Jolt);
	REQUIRE(Scene.GetBackend() == EPhysicsBackend::Jolt);
	REQUIRE(Scene.GetBackendIface() != nullptr);
	REQUIRE(std::string(Scene.GetBackendIface()->GetName()) == "Jolt");
	REQUIRE(Scene.GetBackendIface()->HasRigidWorld());
}

TEST_CASE("World SetPhysicsBackend switches to Jolt", "[physics][jolt][world]")
{
	UWorld World;
	REQUIRE(World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Arcade);

	World.SetPhysicsBackend(EPhysicsBackend::Jolt);
	REQUIRE(World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Jolt);

	const std::size_t Id = World.GetPhysicsScene().AddBody({0, EBodyType::Dynamic, 8.0f, true});
	auto& Body = World.GetPhysicsScene().GetBodies()[Id];
	Body.Position = {0.0f, 2.5f, 0.0f};
	Body.HalfExtents = {0.4f, 0.4f, 0.4f};

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = 0.0f;
	Params.WalkBounds = 50.0f;
	for (int I = 0; I < 180; ++I)
	{
		World.GetPhysicsScene().Step(Params);
	}
	REQUIRE(Body.Position.y < 1.1f);
	REQUIRE(Body.Position.y > 0.2f);
}

TEST_CASE("PhysScene Jolt Step applies gravity and rests on floor", "[physics][jolt]")
{
	FPhysScene Scene(EPhysicsBackend::Jolt);
	const std::size_t Id = Scene.AddBody({0, EBodyType::Dynamic, 10.0f, true});
	auto& Body = Scene.GetBodies()[Id];
	Body.Position = {0.0f, 3.0f, 0.0f};
	Body.HalfExtents = {0.5f, 0.5f, 0.5f};

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = 0.0f;
	Params.WalkBounds = 100.0f;

	for (int I = 0; I < 240; ++I)
	{
		Scene.Step(Params);
	}

	REQUIRE(Body.Position.y < 1.2f);
	REQUIRE(Body.Position.y > 0.3f);
	REQUIRE(std::abs(Body.VelocityY) < 1.0f);
}

TEST_CASE("PhysScene Jolt dynamic rests on static box", "[physics][jolt]")
{
	FPhysScene Scene(EPhysicsBackend::Jolt);

	const std::size_t GroundId = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	auto& Ground = Scene.GetBodies()[GroundId];
	Ground.Position = {0.0f, 0.5f, 0.0f};
	Ground.HalfExtents = {2.0f, 0.5f, 2.0f};

	const std::size_t BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	auto& Box = Scene.GetBodies()[BoxId];
	Box.Position = {0.0f, 4.0f, 0.0f};
	Box.HalfExtents = {0.4f, 0.4f, 0.4f};

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = -10.0f; // below ground so static box is the support
	Params.WalkBounds = 100.0f;

	for (int I = 0; I < 300; ++I)
	{
		Scene.Step(Params);
	}

	// Dynamic COM should settle near ground top (1.0) + halfExtents (0.4) ≈ 1.4
	REQUIRE_THAT(Box.Position.y, WithinAbs(1.4f, 0.35f));
	REQUIRE(std::abs(Box.VelocityY) < 1.5f);
}

TEST_CASE("PhysScene Jolt dynamic rests on TriangleMesh static", "[physics][jolt][mesh]")
{
	ULevel Level;
	FMeshData Data;
	// Flat plane at y=1 covering xz [-3,3]
	Data.Vertices.push_back({{-3.0f, 1.0f, -3.0f}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1}});
	Data.Vertices.push_back({{3.0f, 1.0f, -3.0f}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1}});
	Data.Vertices.push_back({{3.0f, 1.0f, 3.0f}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1}});
	Data.Vertices.push_back({{-3.0f, 1.0f, 3.0f}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1}});
	Data.Indices = {0, 1, 2, 0, 2, 3};
	Data.Submeshes.push_back({0, 6, 0});

	UStaticMeshComponent Component{};
	Component.Mesh = std::make_shared<UStaticMesh>(UStaticMesh::CreateCpu(Data));
	Component.bCollisionEnabled = true;
	Level.GetStaticMeshes().push_back(std::move(Component));

	FPhysScene Scene(EPhysicsBackend::Jolt);
	Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	const std::size_t BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	auto& Box = Scene.GetBodies()[BoxId];
	Box.Position = {0.0f, 5.0f, 0.0f};
	Box.HalfExtents = {0.35f, 0.35f, 0.35f};

	Scene.SyncFromLevel(Level);
	REQUIRE(Scene.GetBodies()[0].CollisionShape == ECollisionShape::TriangleMesh);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = -20.0f;
	Params.WalkBounds = 100.0f;

	for (int I = 0; I < 360; ++I)
	{
		Scene.Step(Params);
	}

	// FPlane at y=1 + halfExtents 0.35 ≈ 1.35
	REQUIRE_THAT(Box.Position.y, WithinAbs(1.35f, 0.45f));
	REQUIRE(std::abs(Box.VelocityY) < 2.0f);
}

TEST_CASE("PhysScene Jolt LineTrace hits static box", "[physics][jolt][trace]")
{
	FPhysScene Scene(EPhysicsBackend::Jolt);
	REQUIRE(Scene.GetBackendIface()->HasNarrowPhaseTraces());

	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	auto& Body = Scene.GetBodies()[Id];
	Body.Position = {0.0f, 0.5f, 0.0f};
	Body.HalfExtents = {0.5f, 0.5f, 0.5f};

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	REQUIRE(Scene.LineTraceSingleByChannel(
		Hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f}, ECollisionChannel::WorldStatic, Params));
	REQUIRE(Hit.bBlockingHit);
	REQUIRE_THAT(Hit.ImpactPoint.z, WithinAbs(-0.5f, 0.08f));
	REQUIRE(Hit.ImpactNormal.z < -0.5f);
}

TEST_CASE("PhysScene Jolt SphereTrace hits static box", "[physics][jolt][trace]")
{
	FPhysScene Scene(EPhysicsBackend::Jolt);

	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	auto& Body = Scene.GetBodies()[Id];
	Body.Position = {0.0f, 0.5f, 0.0f};
	Body.HalfExtents = {0.5f, 0.5f, 0.5f};

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	REQUIRE(Scene.SphereTraceSingleByChannel(
		Hit, {0.0f, 0.5f, -3.0f}, {0.0f, 0.5f, 3.0f}, 0.25f, ECollisionChannel::WorldStatic, Params));
	REQUIRE(Hit.bBlockingHit);
	// Sweep center stops before the face by ~radius.
	REQUIRE_THAT(Hit.Location.z, WithinAbs(-0.75f, 0.12f));
}

#else

TEST_CASE("PhysScene Jolt disabled falls back to Arcade", "[physics][jolt]")
{
	FPhysScene scene(EPhysicsBackend::Jolt);
	REQUIRE(scene.GetBackend() == EPhysicsBackend::Arcade);
}

#endif
