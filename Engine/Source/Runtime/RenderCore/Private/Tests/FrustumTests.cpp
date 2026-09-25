#include "CoreMinimal.h"
#include "Frustum.h"
#include "Migration/GlmInterop.h"
#include "Misc/AutomationTest.h"

#include <glm/gtc/matrix_transform.hpp>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformLocalBoxTest, "System.RenderCore.Frustum.TransformLocalBox",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTransformLocalBoxTest::RunTest(const FString& Parameters)
{
	// A unit box turned 45 degrees about the vertical axis grows in X and keeps its height.
	const FMatrix Model = FromGlm(glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	const FBox Box = TransformLocalBox(FVector(-0.5f), FVector(0.5f), Model);
	TestTrue("Min X grows", Box.Min.X < -0.5f);
	TestTrue("Max X grows", Box.Max.X > 0.5f);
	TestTrue("Min Y kept", Box.Min.Y <= -0.5f + 1.0e-4f);
	TestTrue("Max Y kept", Box.Max.Y >= 0.5f - 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineBoxIntersectionTest, "System.RenderCore.Frustum.LineBoxIntersection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLineBoxIntersectionTest::RunTest(const FString& Parameters)
{
	// A segment from +Z hits the unit cube only when it points at it.
	const FBox Box(FVector(-0.5f), FVector(0.5f));
	const FVector Start(0.0f, 0.0f, 5.0f);

	const FVector Down(0.0f, 0.0f, -10.0f);
	TestTrue("Toward the box", FMath::LineBoxIntersection(Box, Start, Start + Down, Down));

	const FVector Up(0.0f, 0.0f, 10.0f);
	TestFalse("Away from the box", FMath::LineBoxIntersection(Box, Start, Start + Up, Up));

	const FVector Beside(2.0f, 0.0f, 5.0f);
	TestFalse("Beside the box", FMath::LineBoxIntersection(Box, Beside, Beside + Down, Down));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrustumIntersectsAabbTest, "System.RenderCore.Frustum.IntersectsAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrustumIntersectsAabbTest::RunTest(const FString& Parameters)
{
	// The renderer's clip transform (OpenGL conventions, built with glm until the renderer migrates).
	const glm::mat4 View = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 Proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(FromGlm(Proj * View));

	TestTrue("Box at the origin", Frustum.IntersectsAabb(FBox(FVector(-0.5f), FVector(0.5f))));
	TestFalse("Box far away", Frustum.IntersectsAabb(FBox(FVector(200.0f), FVector(201.0f))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
