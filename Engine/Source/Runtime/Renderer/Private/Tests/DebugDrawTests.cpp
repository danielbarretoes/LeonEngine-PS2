#include <catch2/catch_test_macros.hpp>
#include "Debug/DebugDraw.h"

TEST_CASE("DebugDraw batch accumulates and clears", "[debug][draw]") {
    leon::DebugDraw draw;
    REQUIRE(draw.IsEmpty());

    draw.AddLine({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    REQUIRE_FALSE(draw.IsEmpty());

    draw.Clear();
    REQUIRE(draw.IsEmpty());

    draw.AddAabb({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
    // AABB = 12 edges × 2 verts
    REQUIRE_FALSE(draw.IsEmpty());

    draw.AddArrow({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    REQUIRE_FALSE(draw.IsEmpty());

    draw.Clear();
    REQUIRE(draw.IsEmpty());
}
