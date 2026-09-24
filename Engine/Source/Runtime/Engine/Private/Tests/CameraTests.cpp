#include "Camera/CameraComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/geometric.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("Camera orbit clamps pitch", "[core][camera]")
{
	UCameraComponent Cam;
	Cam.SetYawPitch(0.0f, 0.0f);
	Cam.Orbit(0.0f, 200.0f);
	REQUIRE_THAT(Cam.GetPitchDegrees(), WithinAbs(89.0f, 1.0e-4f));
	Cam.Orbit(0.0f, -400.0f);
	REQUIRE_THAT(Cam.GetPitchDegrees(), WithinAbs(-89.0f, 1.0e-4f));
}

TEST_CASE("Camera orbit distance clamps and FreeLook ignores zoom", "[core][camera]")
{
	UCameraComponent Cam;
	Cam.SetDistance(5.0f);
	Cam.Zoom(100.0f);
	REQUIRE_THAT(Cam.GetDistance(), WithinAbs(0.5f, 1.0e-4f));

	Cam.SetDistance(5.0f);
	Cam.SetMode(ECameraMode::FreeLook);
	Cam.Zoom(2.0f);
	REQUIRE_THAT(Cam.GetDistance(), WithinAbs(5.0f, 1.0e-4f));
}

TEST_CASE("Camera orbit position follows target and distance", "[core][camera]")
{
	UCameraComponent Cam;
	Cam.SetMode(ECameraMode::Orbit);
	Cam.SetTarget({0.0f, 0.0f, 0.0f});
	Cam.SetYawPitch(0.0f, 0.0f);
	Cam.SetDistance(4.0f);

	const glm::vec3 Eye = Cam.GetCameraLocation();
	REQUIRE_THAT(glm::length(Eye), WithinAbs(4.0f, 1.0e-3f));
	REQUIRE_THAT(glm::length(Cam.ForwardVector()), WithinAbs(1.0f, 1.0e-4f));
	REQUIRE_THAT(glm::length(Cam.RightVector()), WithinAbs(1.0f, 1.0e-4f));
}

TEST_CASE("Camera FreeLook uses eye location", "[core][camera]")
{
	UCameraComponent Cam;
	Cam.SetMode(ECameraMode::FreeLook);
	Cam.SetEyeLocation({1.0f, 2.0f, 3.0f});
	const glm::vec3 Eye = Cam.GetCameraLocation();
	REQUIRE_THAT(Eye.x, WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(Eye.y, WithinAbs(2.0f, 1.0e-5f));
	REQUIRE_THAT(Eye.z, WithinAbs(3.0f, 1.0e-5f));
}
