#include "Camera/CameraComponent.h"
#include "GameFramework/Input.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/geometric.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("MoveAxes2D any detects nonzero", "[core][input]")
{
	FMoveAxes2D Zero{};
	REQUIRE_FALSE(Zero.Any());
	REQUIRE(FMoveAxes2D{1.0f, 0.0f}.Any());
	REQUIRE(FMoveAxes2D{0.0f, -1.0f}.Any());
}

TEST_CASE("yawRelativeMoveXZ returns zero without axes", "[core][input]")
{
	const glm::vec3 Move = YawRelativeMoveXz(45.0f, {});
	REQUIRE_THAT(Move.x, WithinAbs(0.0f, 1.0e-6f));
	REQUIRE_THAT(Move.y, WithinAbs(0.0f, 1.0e-6f));
	REQUIRE_THAT(Move.z, WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("yawRelativeMoveXZ forward at yaw 0", "[core][input]")
{
	const glm::vec3 Move = YawRelativeMoveXz(0.0f, {0.0f, 1.0f});
	REQUIRE_THAT(glm::length(Move), WithinAbs(1.0f, 1.0e-4f));
	REQUIRE_THAT(Move.x, WithinAbs(-1.0f, 1.0e-4f));
	REQUIRE_THAT(Move.y, WithinAbs(0.0f, 1.0e-4f));
	REQUIRE_THAT(Move.z, WithinAbs(0.0f, 1.0e-4f));
}

TEST_CASE("cameraRelativeMoveXZ matches camera yaw", "[core][input]")
{
	UCameraComponent Cam;
	Cam.SetYawPitch(90.0f, 0.0f);
	const glm::vec3 A = CameraRelativeMoveXz(Cam, {0.0f, 1.0f});
	const glm::vec3 B = YawRelativeMoveXz(90.0f, {0.0f, 1.0f});
	REQUIRE_THAT(A.x, WithinAbs(B.x, 1.0e-5f));
	REQUIRE_THAT(A.z, WithinAbs(B.z, 1.0e-5f));
}
