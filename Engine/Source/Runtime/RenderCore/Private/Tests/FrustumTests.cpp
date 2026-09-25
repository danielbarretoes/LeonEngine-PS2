#include "CoreMinimal.h"
#include "Frustum.h"
#include "GLClipSpace.h"
#include "Misc/AutomationTest.h"
#include "ViewMatrices.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformLocalBoxTest, "System.RenderCore.Frustum.TransformLocalBox",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTransformLocalBoxTest::RunTest(const FString& Parameters)
{
	// A 100 cm box turned 45 degrees about the vertical axis grows in X and keeps its height.
	const FMatrix Model = FQuatRotationMatrix(FQuat(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(45.0f)));
	const FBox Box = TransformLocalBox(FVector(-50.0f), FVector(50.0f), Model);
	TestTrue("Min X grows", Box.Min.X < -50.0f);
	TestTrue("Max X grows", Box.Max.X > 50.0f);
	TestTrue("Min Z kept", Box.Min.Z >= -50.0f - 1.0e-2f && Box.Min.Z <= -50.0f + 1.0e-2f);
	TestTrue("Max Z kept", Box.Max.Z >= 50.0f - 1.0e-2f && Box.Max.Z <= 50.0f + 1.0e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineBoxIntersectionTest, "System.RenderCore.Frustum.LineBoxIntersection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLineBoxIntersectionTest::RunTest(const FString& Parameters)
{
	// A segment from +Z hits the 100 cm cube only when it points at it.
	const FBox Box(FVector(-50.0f), FVector(50.0f));
	const FVector Start(0.0f, 0.0f, 500.0f);

	const FVector Down(0.0f, 0.0f, -1000.0f);
	TestTrue("Toward the box", FMath::LineBoxIntersection(Box, Start, Start + Down, Down));

	const FVector Up(0.0f, 0.0f, 1000.0f);
	TestFalse("Away from the box", FMath::LineBoxIntersection(Box, Start, Start + Up, Up));

	const FVector Beside(200.0f, 0.0f, 500.0f);
	TestFalse("Beside the box", FMath::LineBoxIntersection(Box, Beside, Beside + Down, Down));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrustumIntersectsAabbTest, "System.RenderCore.Frustum.IntersectsAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrustumIntersectsAabbTest::RunTest(const FString& Parameters)
{
	// The renderer's clip transform: a UE view, a UE perspective (near 10 cm, far 100 m), then GL clip space.
	// Looking at the origin from 5 m along +Y, Z up.
	const FMatrix View = MakeLookAtView(FVector(0.0f, 500.0f, 0.0f), FVector(0.0f), FVector(0.0f, 0.0f, 1.0f));
	const float HalfFov = FMath::DegreesToRadians(60.0f) / 2.0f;
	const FMatrix Proj = ToGLClipSpace(FPerspectiveMatrix(HalfFov, HalfFov, 1.0f, 1.0f, 10.0f, 10000.0f));

	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(View * Proj);

	TestTrue("Box at the origin", Frustum.IntersectsAabb(FBox(FVector(-50.0f), FVector(50.0f))));
	TestFalse("Box far away", Frustum.IntersectsAabb(FBox(FVector(20000.0f), FVector(20100.0f))));
	TestFalse("Box behind the camera",
		Frustum.IntersectsAabb(FBox(FVector(-50.0f, 600.0f, -50.0f), FVector(50.0f, 700.0f, 50.0f))));
	TestFalse("Box before the near plane",
		Frustum.IntersectsAabb(FBox(FVector(-1.0f, 495.0f, -1.0f), FVector(1.0f, 496.0f, 1.0f))));
	TestFalse("Box past the far plane",
		Frustum.IntersectsAabb(FBox(FVector(-50.0f, -9700.0f, -50.0f), FVector(50.0f, -9600.0f, 50.0f))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
