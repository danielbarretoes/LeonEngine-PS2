#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakeCubeTest, "System.RenderCore.Primitives.MakeCube",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakeCubeTest::RunTest(const FString& Parameters)
{
	const FMeshData Cube = MakeCube();
	TestFalse("Not empty", Cube.IsEmpty());
	TestEqual("Vertices", Cube.Vertices.Num(), 24);
	TestEqual("Indices", Cube.Indices.Num(), 36);
	for (const FVertex& V : Cube.Vertices)
	{
		TestTrue("Inside the 100 cm box",
			FMath::Abs(V.Position.X) <= 50.0f + 1.0e-3f && FMath::Abs(V.Position.Y) <= 50.0f + 1.0e-3f &&
				FMath::Abs(V.Position.Z) <= 50.0f + 1.0e-3f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakePlaneTest, "System.RenderCore.Primitives.MakePlane",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakePlaneTest::RunTest(const FString& Parameters)
{
	// The plane lies on XZ.
	const FMeshData Plane = MakePlane(200.0f);
	TestFalse("Not empty", Plane.IsEmpty());
	for (const FVertex& V : Plane.Vertices)
	{
		TestEqual("Y", V.Position.Y, 0.0f, 1.0e-5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakeSphereTest, "System.RenderCore.Primitives.MakeSphere",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakeSphereTest::RunTest(const FString& Parameters)
{
	// Too-low tessellation is clamped to a valid mesh.
	const FMeshData Sphere = MakeSphere(2, 1);
	TestFalse("Not empty", Sphere.IsEmpty());
	TestEqual("Triangles", Sphere.Indices.Num() % 3, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
