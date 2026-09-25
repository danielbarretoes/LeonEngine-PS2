#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraOrbitClampsPitchTest, "System.Engine.Camera.OrbitClampsPitch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCameraOrbitClampsPitchTest::RunTest(const FString& Parameters)
{
	// Orbiting past the poles clamps the pitch to +/-89 degrees.
	UCameraComponent Cam;
	Cam.SetYawPitch(0.0f, 0.0f);
	Cam.Orbit(0.0f, 200.0f);
	TestEqual("Pitch clamped up", Cam.GetPitchDegrees(), 89.0f, 1.0e-4f);
	Cam.Orbit(0.0f, -400.0f);
	TestEqual("Pitch clamped down", Cam.GetPitchDegrees(), -89.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraOrbitDistanceClampsAndFreeLookIgnoresZoomTest,
	"System.Engine.Camera.OrbitDistanceClampsAndFreeLookIgnoresZoom",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCameraOrbitDistanceClampsAndFreeLookIgnoresZoomTest::RunTest(const FString& Parameters)
{
	// Zooming in past the minimum clamps the orbit distance; FreeLook leaves the distance alone.
	UCameraComponent Cam;
	Cam.SetDistance(5.0f);
	Cam.Zoom(100.0f);
	TestEqual("Orbit distance clamped", Cam.GetDistance(), 0.5f, 1.0e-4f);

	Cam.SetDistance(5.0f);
	Cam.SetMode(ECameraMode::FreeLook);
	Cam.Zoom(2.0f);
	TestEqual("FreeLook distance unchanged", Cam.GetDistance(), 5.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraOrbitPositionFollowsTargetAndDistanceTest,
	"System.Engine.Camera.OrbitPositionFollowsTargetAndDistance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCameraOrbitPositionFollowsTargetAndDistanceTest::RunTest(const FString& Parameters)
{
	// The orbit eye sits at the distance from the target, and the view basis vectors are unit length.
	UCameraComponent Cam;
	Cam.SetMode(ECameraMode::Orbit);
	Cam.SetTarget(FVector::ZeroVector);
	Cam.SetYawPitch(0.0f, 0.0f);
	Cam.SetDistance(4.0f);

	const FVector Eye = Cam.GetCameraLocation();
	TestEqual("Eye distance", Eye.Size(), 4.0f, 1.0e-3f);
	TestEqual("Forward is unit", Cam.ForwardVector().Size(), 1.0f, 1.0e-4f);
	TestEqual("Right is unit", Cam.RightVector().Size(), 1.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraFreeLookUsesEyeLocationTest, "System.Engine.Camera.FreeLookUsesEyeLocation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCameraFreeLookUsesEyeLocationTest::RunTest(const FString& Parameters)
{
	// In FreeLook the camera location is the explicit eye location.
	UCameraComponent Cam;
	Cam.SetMode(ECameraMode::FreeLook);
	Cam.SetEyeLocation(FVector(1.0f, 2.0f, 3.0f));
	const FVector Eye = Cam.GetCameraLocation();
	TestEqual("Eye X", Eye.X, 1.0f, 1.0e-5f);
	TestEqual("Eye Y", Eye.Y, 2.0f, 1.0e-5f);
	TestEqual("Eye Z", Eye.Z, 3.0f, 1.0e-5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
