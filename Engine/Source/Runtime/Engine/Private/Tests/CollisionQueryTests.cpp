#include "Physics/PhysScene.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("LineTraceSingleByChannel hits static AABB", "[physics][trace]")
{
	FPhysScene Scene;
	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = {0.0f, 0.5f, 0.0f};
	Scene.GetBodies()[Id].HalfExtents = {0.5f, 0.5f, 0.5f};

	FHitResult Hit{};
	REQUIRE(
		Scene.LineTraceSingleByChannel(Hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f}, ECollisionChannel::WorldStatic));
	REQUIRE(Hit.bBlockingHit);
	REQUIRE(Hit.Time < 1.0f);
	REQUIRE_THAT(Hit.ImpactNormal.z, WithinAbs(-1.0f, 1.0e-3f));
}

TEST_CASE("LineTraceSingleByChannel filters by channel", "[physics][trace]")
{
	FPhysScene Scene;
	const std::size_t Id = Scene.AddBody({0, EBodyType::Dynamic, 1.0f, true});
	Scene.GetBodies()[Id].Position = {0.0f, 0.5f, 0.0f};
	Scene.GetBodies()[Id].HalfExtents = {0.5f, 0.5f, 0.5f};

	FHitResult Hit{};
	REQUIRE_FALSE(
		Scene.LineTraceSingleByChannel(Hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f}, ECollisionChannel::WorldStatic));
	REQUIRE(
		Scene.LineTraceSingleByChannel(Hit, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f}, ECollisionChannel::WorldDynamic));
}

TEST_CASE("SphereTraceSingleByChannel hits floor plane", "[physics][trace]")
{
	FPhysScene Scene;
	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorY = 0.0f;

	FHitResult Hit{};
	REQUIRE(Scene.SphereTraceSingleByChannel(
		Hit, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.35f, ECollisionChannel::Visibility, Params));
	REQUIRE(Hit.bFloorPlane);
	REQUIRE_THAT(Hit.ImpactPoint.y, WithinAbs(0.0f, 1.0e-3f));
	REQUIRE(Hit.ImpactNormal.y > 0.5f);
}

TEST_CASE("CapsuleTraceSingleByChannel finds platform top", "[physics][trace]")
{
	FPhysScene Scene;
	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = {0.0f, 1.0f, 0.0f};
	Scene.GetBodies()[Id].HalfExtents = {1.0f, 1.0f, 1.0f}; // top at y=2

	FHitResult Hit{};
	const float Radius = 0.35f;
	const float HalfHeight = 0.5f;
	REQUIRE(Scene.CapsuleTraceSingleByChannel(
		Hit, {0.0f, 3.0f, 0.0f}, {0.0f, 1.5f, 0.0f}, Radius, HalfHeight, ECollisionChannel::Visibility));
	REQUIRE(Hit.bBlockingHit);
	REQUIRE(Hit.ImpactPoint.y <= 2.0f + 1.0e-2f);
	REQUIRE(Hit.ImpactNormal.y > 0.5f);
}

TEST_CASE("LineTraceMultiByChannel returns all hits sorted", "[physics][trace][multi]")
{
	FPhysScene Scene;
	const std::size_t NearId = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[NearId].Position = {0.0f, 0.5f, 0.0f};
	Scene.GetBodies()[NearId].HalfExtents = {0.5f, 0.5f, 0.5f};

	const std::size_t FarId = Scene.AddBody({1, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[FarId].Position = {0.0f, 0.5f, 3.0f};
	Scene.GetBodies()[FarId].HalfExtents = {0.5f, 0.5f, 0.5f};

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = false;

	std::vector<FHitResult> Hits;
	REQUIRE(Scene.LineTraceMultiByChannel(
		Hits, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 5.0f}, ECollisionChannel::WorldStatic, Params));
	REQUIRE(Hits.size() == 2);
	REQUIRE(Hits[0].Time < Hits[1].Time);
	REQUIRE(Hits[0].LevelMeshIndex == 0);
	REQUIRE(Hits[1].LevelMeshIndex == 1);

	FHitResult Single{};
	REQUIRE(Scene.LineTraceSingleByChannel(
		Single, {0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 5.0f}, ECollisionChannel::WorldStatic, Params));
	REQUIRE_THAT(Single.Time, WithinAbs(Hits[0].Time, 1.0e-5f));
}

TEST_CASE("SphereTraceMultiByChannel includes floor and bodies", "[physics][trace][multi]")
{
	FPhysScene Scene;
	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = {0.0f, 2.0f, 0.0f};
	Scene.GetBodies()[Id].HalfExtents = {1.0f, 0.5f, 1.0f}; // top at 2.5, bottom at 1.5

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorY = 0.0f;

	std::vector<FHitResult> Hits;
	REQUIRE(Scene.SphereTraceMultiByChannel(
		Hits, {0.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.25f, ECollisionChannel::Visibility, Params));
	REQUIRE(Hits.size() >= 2);
	REQUIRE(Hits.front().Time <= Hits.back().Time);
	bool bAnyFloor = false;
	bool bAnyBody = false;
	for (const FHitResult& H : Hits)
	{
		bAnyFloor = bAnyFloor || H.bFloorPlane;
		bAnyBody = bAnyBody || !H.bFloorPlane;
	}
	REQUIRE(bAnyFloor);
	REQUIRE(bAnyBody);
}

TEST_CASE("CapsuleTraceMultiByChannel returns multiple blocking hits", "[physics][trace][multi]")
{
	FPhysScene Scene;
	const std::size_t A = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[A].Position = {0.0f, 1.0f, 0.0f};
	Scene.GetBodies()[A].HalfExtents = {0.5f, 0.5f, 0.5f};
	const std::size_t B = Scene.AddBody({1, EBodyType::Dynamic, 1.0f, true});
	Scene.GetBodies()[B].Position = {0.0f, 3.0f, 0.0f};
	Scene.GetBodies()[B].HalfExtents = {0.5f, 0.5f, 0.5f};

	std::vector<FHitResult> Hits;
	REQUIRE(Scene.CapsuleTraceMultiByChannel(
		Hits, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.2f, 0.3f, ECollisionChannel::Visibility));
	REQUIRE(Hits.size() == 2);
	REQUIRE(Hits[0].Time < Hits[1].Time);
}
