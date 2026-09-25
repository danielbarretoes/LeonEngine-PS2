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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraUEViewAndProjectionTest, "System.Engine.Camera.UEViewAndProjection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCameraUEViewAndProjectionTest::RunTest(const FString& Parameters)
{
	// The view is UE's (x right, y up, z forward) and the projections give UE depth: 0 at near, 1 at far.
	UCameraComponent Cam;
	Cam.SetPerspective(60.0f, 2.0f, 0.5f, 50.0f);
	Cam.SetMode(ECameraMode::Orbit);
	Cam.SetTarget(FVector(1.0f, 0.5f, -2.0f));
	Cam.SetYawPitch(30.0f, 20.0f);
	Cam.SetDistance(6.0f);

	const FMatrix View = Cam.ViewMatrix();
	const FVector Eye = Cam.GetCameraLocation();
	const FVector Ahead = FVector(View.TransformPosition(Eye + (Cam.ForwardVector() * 3.0f)));
	const FVector Right = FVector(View.TransformPosition(Eye + Cam.RightVector()));
	TestTrue("Target ahead on +z",
		FVector(View.TransformPosition(Cam.GetTarget())).Equals(FVector(0.0f, 0.0f, 6.0f), 1.0e-4f));
	TestTrue("Forward is +z", Ahead.Equals(FVector(0.0f, 0.0f, 3.0f), 1.0e-4f));
	TestTrue("RightVector is +x", Right.Equals(FVector(1.0f, 0.0f, 0.0f), 1.0e-4f));
	TestTrue("World up is up on screen", FVector(View.TransformVector(FVector(0.0f, 1.0f, 0.0f))).Y > 0.0f);

	const FMatrix& Perspective = Cam.ProjectionMatrix();
	const FVector4 Near = Perspective.TransformFVector4(FVector4(0.0f, 0.0f, 0.5f, 1.0f));
	const FVector4 Far = Perspective.TransformFVector4(FVector4(0.0f, 0.0f, 50.0f, 1.0f));
	TestEqual("Perspective near depth", Near.Z / Near.W, 0.0f, 1.0e-6f);
	TestEqual("Perspective far depth", Far.Z / Far.W, 1.0f, 1.0e-6f);
	// The vertical field of view is kept: the top edge at 30 degrees, the right edge at aspect times as wide.
	const float TanHalf = FMath::Tan(FMath::DegreesToRadians(30.0f));
	const FVector4 Corner = Perspective.TransformFVector4(FVector4(2.0f * TanHalf, TanHalf, 1.0f, 1.0f));
	TestEqual("Right edge", Corner.X / Corner.W, 1.0f, 1.0e-5f);
	TestEqual("Top edge", Corner.Y / Corner.W, 1.0f, 1.0e-5f);

	Cam.SetOrthographic(10.0f, 2.0f, 0.5f, 50.0f);
	const FMatrix& Ortho = Cam.ProjectionMatrix();
	TestTrue("Ortho near corner",
		FVector(Ortho.TransformPosition(FVector(10.0f, 5.0f, 0.5f))).Equals(FVector(1.0f, 1.0f, 0.0f), 1.0e-5f));
	TestEqual("Ortho far depth", Ortho.TransformPosition(FVector(0.0f, 0.0f, 50.0f)).Z, 1.0f, 1.0e-5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
