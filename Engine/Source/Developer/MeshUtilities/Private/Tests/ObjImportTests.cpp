#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjImport.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjImportCubeFixtureTest, "System.MeshUtilities.ObjImport.CubeFixture",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FObjImportCubeFixtureTest::RunTest(const FString& Parameters)
{
	// The Cube fixture imports as twelve triangles.
	const FString Fixture =
		FPaths::Combine(FPaths::EngineSourceDir(), "Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj");
	if (!TestTrue("Fixture exists", FPaths::FileExists(Fixture)))
	{
		return false;
	}

	const FMeshData Data = LoadObj(Fixture);
	TestFalse("Not empty", Data.IsEmpty());
	TestEqual("Indices", Data.Indices.Num(), 36);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjImportMinimalObjTest, "System.MeshUtilities.ObjImport.MinimalObj",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FObjImportMinimalObjTest::RunTest(const FString& Parameters)
{
	// A one-triangle OBJ written to a temporary file loads as whole triangles.
	const FString Path = FPaths::CreateTempFilename(*FPaths::EngineIntermediateDir(), "leon_test_tri", ".obj");
	if (!TestTrue(
			"Written", FFileHelper::SaveStringToFile("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n", *Path)))
	{
		return false;
	}
	const FMeshData Data = LoadObj(Path);
	TestFalse("Not empty", Data.IsEmpty());
	TestEqual("Whole triangles", Data.Indices.Num() % 3, 0);
	IFileManager::Get().Delete(*Path);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
