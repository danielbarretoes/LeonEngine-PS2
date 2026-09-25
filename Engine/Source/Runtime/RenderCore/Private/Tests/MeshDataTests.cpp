#include "CoreMinimal.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComputeTangentsTest, "System.RenderCore.MeshData.ComputeTangents",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComputeTangentsTest::RunTest(const FString& Parameters)
{
	// Recomputed from the cube's UVs, every tangent is a unit vector with a +-1 handedness.
	FMeshData Data = MakeCube();
	for (FVertex& V : Data.Vertices)
	{
		V.Tangent = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	ComputeTangents(Data);
	for (const FVertex& V : Data.Vertices)
	{
		TestEqual("Unit tangent", FVector(V.Tangent).Size(), 1.0f, 1.0e-3f);
		TestTrue("Handedness", V.Tangent.W == 1.0f || V.Tangent.W == -1.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
