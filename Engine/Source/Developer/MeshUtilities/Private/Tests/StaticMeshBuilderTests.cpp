#include "CoreMinimal.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshBuilder.h"
#include "Tests/GltfTestCube.h"
#include "Tests/LegacyCoordinateConversion.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticMeshBuilderImportMatchesLegacyTest,
	"System.MeshUtilities.StaticMeshBuilder.ImportMatchesLegacy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStaticMeshBuilderImportMatchesLegacyTest::RunTest(const FString& Parameters)
{
	// Built from a glTF (converted at import), a mesh is the one the legacy cook gave: the file's data in its own
	// space, converted as a version 1 .lmesh was when it loaded. Only glTF is a mesh source.
	FString Error;
	FMeshData Built;
	if (!TestTrue(
			"Cube.glb built", FStaticMeshBuilder::BuildFromFile(GetGltfFixturePath(TEXT("Cube.glb")), Built, Error)))
	{
		AddError(Error);
		return false;
	}
	FMeshData Legacy = MakeGltfTestCubeSourceSpace();
	FLegacyCoordinateConversion::ConvertMeshData(Legacy);
	if (TestEqual("Vertices", Built.Vertices.Num(), Legacy.Vertices.Num()) &&
		TestTrue("Indices", Built.Indices == Legacy.Indices))
	{
		for (int32 Index = 0; Index < Built.Vertices.Num(); ++Index)
		{
			const FVertex& A = Built.Vertices[Index];
			const FVertex& B = Legacy.Vertices[Index];
			if (!A.Position.Equals(B.Position, 1.0e-4f) || !A.Normal.Equals(B.Normal, 1.0e-4f))
			{
				AddError(FString::Printf("Vertex %d differs", Index));
				break;
			}
		}
	}
	TestEqual("One section", Built.Submeshes.Num(), 1);
	TestEqual("One slot per material", Built.MaterialSlotNames.Num(), Built.Materials.Num());
	TestTrue("An unnamed slot", Built.MaterialSlotNames.Num() == 1 && Built.MaterialSlotNames[0].IsEmpty());

	FMeshData Unused;
	TestTrue("glb and gltf only",
		FStaticMeshBuilder::IsSupportedExtension(TEXT("glb")) &&
			FStaticMeshBuilder::IsSupportedExtension(TEXT(".gltf")) &&
			!FStaticMeshBuilder::IsSupportedExtension(TEXT("obj")) &&
			!FStaticMeshBuilder::IsSupportedExtension(TEXT("fbx")));
	TestFalse("Not a mesh source", FStaticMeshBuilder::BuildFromFile(TEXT("Mesh.fbx"), Unused, Error));
	TestTrue("Explained", Error.Contains(TEXT("Not a mesh source")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
