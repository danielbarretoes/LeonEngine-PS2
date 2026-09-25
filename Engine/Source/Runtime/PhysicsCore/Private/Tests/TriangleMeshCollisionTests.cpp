#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSegmentTriangleTest, "System.PhysicsCore.Triangle.SegmentTriangle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSegmentTriangleTest::RunTest(const FString& Parameters)
{
	// A vertical segment hits a unit floor triangle two thirds of the way down, with the normal facing up.
	const FVector V0(-1.0f, 0.0f, -1.0f);
	const FVector V1(1.0f, 0.0f, -1.0f);
	const FVector V2(0.0f, 0.0f, 1.0f);
	float T = 1.0f;
	FVector N = FVector::ZeroVector;
	TestTrue("Hit", SegmentTriangle(FVector(0.0f, 2.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), V0, V1, V2, T, N));
	TestEqual("Time", T, 2.0f / 3.0f, 1.0e-3f);
	TestTrue("Normal up", N.Y > 0.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
