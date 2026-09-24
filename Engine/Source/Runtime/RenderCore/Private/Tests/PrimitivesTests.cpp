#include "Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;

TEST_CASE("MakeCube has expected topology and bounds", "[render][primitives]")
{
	const FMeshData Cube = MakeCube();
	REQUIRE_FALSE(Cube.empty());
	REQUIRE(Cube.Vertices.size() == 24);
	REQUIRE(Cube.Indices.size() == 36);

	for (const auto& V : Cube.Vertices)
	{
		REQUIRE(std::abs(V.Position.x) <= 0.5f + 1.0e-4f);
		REQUIRE(std::abs(V.Position.y) <= 0.5f + 1.0e-4f);
		REQUIRE(std::abs(V.Position.z) <= 0.5f + 1.0e-4f);
	}
}

TEST_CASE("MakePlane lies on XZ", "[render][primitives]")
{
	const FMeshData Plane = MakePlane(2.0f);
	REQUIRE_FALSE(Plane.empty());
	for (const auto& V : Plane.Vertices)
	{
		REQUIRE_THAT(V.Position.y, WithinAbs(0.0f, 1.0e-5f));
	}
}

TEST_CASE("MakeSphere clamps low tessellation", "[render][primitives]")
{
	const FMeshData Sphere = MakeSphere(2, 1);
	REQUIRE_FALSE(Sphere.empty());
	REQUIRE(Sphere.Indices.size() % 3 == 0);
}
