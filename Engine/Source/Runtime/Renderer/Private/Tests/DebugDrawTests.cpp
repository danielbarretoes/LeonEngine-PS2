#include "Debug/DebugDraw.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DebugDraw batch accumulates and clears", "[debug][draw]")
{
	FDebugDraw Draw;
	REQUIRE(Draw.IsEmpty());

	Draw.AddLine({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
	REQUIRE_FALSE(Draw.IsEmpty());

	Draw.Clear();
	REQUIRE(Draw.IsEmpty());

	Draw.AddAabb({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
	// AABB = 12 edges × 2 verts
	REQUIRE_FALSE(Draw.IsEmpty());

	Draw.AddArrow({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
	REQUIRE_FALSE(Draw.IsEmpty());

	Draw.Clear();
	REQUIRE(Draw.IsEmpty());
}
