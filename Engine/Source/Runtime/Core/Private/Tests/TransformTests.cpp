#include "Math/Transform.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("Transform identity modelMatrix", "[core][transform]")
{
	FTransform T;
	const glm::mat4 M = T.ModelMatrix();
	REQUIRE_THAT(M[0][0], WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(M[1][1], WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(M[2][2], WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(M[3][0], WithinAbs(0.0f, 1.0e-5f));
	REQUIRE_THAT(M[3][1], WithinAbs(0.0f, 1.0e-5f));
	REQUIRE_THAT(M[3][2], WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("Transform applies translation and yaw", "[core][transform]")
{
	FTransform T;
	T.Position = {2.0f, 3.0f, 4.0f};
	T.RotationDegrees = {0.0f, 90.0f, 0.0f};

	const glm::mat4 M = T.ModelMatrix();
	REQUIRE_THAT(M[3].x, WithinAbs(2.0f, 1.0e-4f));
	REQUIRE_THAT(M[3].y, WithinAbs(3.0f, 1.0e-4f));
	REQUIRE_THAT(M[3].z, WithinAbs(4.0f, 1.0e-4f));
	// 90° yaw: local +X → world -Z
	REQUIRE_THAT(M[0].x, WithinAbs(0.0f, 1.0e-4f));
	REQUIRE_THAT(M[0].z, WithinAbs(-1.0f, 1.0e-4f));
}

TEST_CASE("Transform sanitizes near-zero scale", "[core][transform]")
{
	FTransform T;
	T.Scale = {0.0f, 1.0f, 0.0f};
	const glm::mat4 M = T.ModelMatrix();
	REQUIRE(std::abs(M[0][0]) >= 1.0e-4f);
	REQUIRE(std::abs(M[2][2]) >= 1.0e-4f);
	// normalMatrix should remain finite / non-singular enough
	const glm::mat3 N = T.NormalMatrix();
	REQUIRE(std::isfinite(N[0][0]));
	REQUIRE(std::isfinite(N[1][1]));
}
