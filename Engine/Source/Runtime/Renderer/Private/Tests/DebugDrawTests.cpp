#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugDrawBatchAccumulatesAndClearsTest,
	"System.Renderer.DebugDraw.BatchAccumulatesAndClears",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDebugDrawBatchAccumulatesAndClearsTest::RunTest(const FString& Parameters)
{
	// Lines, boxes and arrows fill the batch and Clear empties it.
	FDebugDraw Draw;
	TestTrue("Starts empty", Draw.IsEmpty());

	Draw.AddLine(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f), FLinearColor(1.0f, 0.0f, 0.0f));
	TestFalse("Line added", Draw.IsEmpty());

	Draw.Clear();
	TestTrue("Cleared", Draw.IsEmpty());

	Draw.AddAabb(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f), FLinearColor(0.0f, 1.0f, 0.0f));
	// AABB = 12 edges x 2 vertices
	TestFalse("Box added", Draw.IsEmpty());

	Draw.AddArrow(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f), FLinearColor(0.0f, 0.0f, 1.0f));
	TestFalse("Arrow added", Draw.IsEmpty());

	Draw.Clear();
	TestTrue("Cleared again", Draw.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
