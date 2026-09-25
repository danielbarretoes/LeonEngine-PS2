#include "Frustum.h"
#include "Math/UnrealMathUtility.h"

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("TransformLocalBox expands under rotation", "[render][frustum]")
{
	const glm::mat4 Model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
	const FBox Box = TransformLocalBox({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, Model);
	REQUIRE(Box.Min.X < -0.5f);
	REQUIRE(Box.Max.X > 0.5f);
	REQUIRE(Box.Min.Y <= -0.5f + 1.0e-4f);
	REQUIRE(Box.Max.Y >= 0.5f - 1.0e-4f);
}

TEST_CASE("LineBoxIntersection hits unit cube from -Z", "[render][frustum]")
{
	const FBox Box(FVector(-0.5f), FVector(0.5f));
	const FVector Start(0.0f, 0.0f, 5.0f);

	const FVector Down = FVector(0.0f, 0.0f, -10.0f);
	REQUIRE(FMath::LineBoxIntersection(Box, Start, Start + Down, Down));

	const FVector Up = FVector(0.0f, 0.0f, 10.0f);
	REQUIRE_FALSE(FMath::LineBoxIntersection(Box, Start, Start + Up, Up));

	const FVector Beside(2.0f, 0.0f, 5.0f);
	REQUIRE_FALSE(FMath::LineBoxIntersection(Box, Beside, Beside + Down, Down));
}

TEST_CASE("Frustum intersectsAabb contains near origin box", "[render][frustum]")
{
	const glm::mat4 View = glm::lookAt(glm::vec3{0.0f, 0.0f, 5.0f}, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
	const glm::mat4 Proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(Proj * View);

	const FBox Inside(FVector(-0.5f), FVector(0.5f));
	REQUIRE(Frustum.IntersectsAabb(Inside));

	const FBox FarAway(FVector(200.0f), FVector(201.0f));
	REQUIRE_FALSE(Frustum.IntersectsAabb(FarAway));
}
