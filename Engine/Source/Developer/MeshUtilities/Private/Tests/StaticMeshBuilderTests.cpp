#include "CoreMinimal.h"
#include "FbxStaticMesh.h"
#include "HAL/FileManager.h"
#include "LegacyCoordinateConversion.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjImportPrivate.h"
#include "StaticMeshBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A quad with UVs and a triangle with mirrored UVs: both tangent handedness signs. */
	const TCHAR* const UvObj = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 1\n"
							   "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
							   "vn 0 0 1\nvn 0 1 0\n"
							   "f 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\nf 1/2/2 5/1/2 2/3/2\n";

	/** Positions, normals, tangents (w too), UVs and indices within Tolerance. */
	void CheckSameMesh(
		FAutomationTestBase& Test, const FString& What, const FMeshData& A, const FMeshData& B, float Tolerance)
	{
		if (!Test.TestEqual(*(What + ": vertex count"), A.Vertices.Num(), B.Vertices.Num()) ||
			!Test.TestTrue(*(What + ": indices"), A.Indices == B.Indices))
		{
			return;
		}
		for (int32 Index = 0; Index < A.Vertices.Num(); ++Index)
		{
			const FVertex& VA = A.Vertices[Index];
			const FVertex& VB = B.Vertices[Index];
			if (!VA.Position.Equals(VB.Position, Tolerance) || !VA.Normal.Equals(VB.Normal, Tolerance) ||
				!FVector(VA.Tangent).Equals(FVector(VB.Tangent), Tolerance) || VA.Tangent.W != VB.Tangent.W ||
				!VA.TexCoord.Equals(VB.TexCoord, Tolerance))
			{
				Test.AddError(FString::Printf("%s: vertex %d differs", *What, Index));
				return;
			}
		}
	}

	/**
	 * A one-triangle ASCII FBX: Header holds the GlobalSettings block (or nothing), Vertices and Normals nine numbers
	 * each.
	 */
	FString MakeTriangleFbx(const FString& Settings, const FString& Vertices, const FString& Normals)
	{
		return "; FBX 7.4.0 project file\n"
			   "FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n}\n" +
			Settings +
			"Objects:  {\n"
			"\tGeometry: 1001, \"Geometry::Tri\", \"Mesh\" {\n"
			"\t\tVertices: *9 {\n\t\t\ta: " +
			Vertices +
			"\n\t\t}\n"
			"\t\tPolygonVertexIndex: *3 {\n\t\t\ta: 0,1,-3\n\t\t}\n"
			"\t\tGeometryVersion: 124\n"
			"\t\tLayerElementNormal: 0 {\n"
			"\t\t\tVersion: 102\n\t\t\tName: \"\"\n"
			"\t\t\tMappingInformationType: \"ByPolygonVertex\"\n\t\t\tReferenceInformationType: \"Direct\"\n"
			"\t\t\tNormals: *9 {\n\t\t\t\ta: " +
			Normals +
			"\n\t\t\t}\n\t\t}\n"
			"\t\tLayer: 0 {\n\t\t\tVersion: 100\n"
			"\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementNormal\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t}\n"
			"\t}\n"
			"\tModel: 2001, \"Model::Tri\", \"Mesh\" {\n\t\tVersion: 232\n\t}\n"
			"}\n"
			"Connections:  {\n\tC: \"OO\",1001,2001\n\tC: \"OO\",2001,0\n}\n";
	}

	/** GlobalSettings declaring the axes (FBX axis index and sign for up, front and right) and the unit in cm. */
	FString MakeFbxSettings(int32 Up, int32 UpSign, int32 Front, int32 FrontSign, int32 UnitScaleFactor)
	{
		return FString::Printf("GlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n"
							   "\t\tP: \"UpAxis\", \"int\", \"Integer\", \"\",%d\n"
							   "\t\tP: \"UpAxisSign\", \"int\", \"Integer\", \"\",%d\n"
							   "\t\tP: \"FrontAxis\", \"int\", \"Integer\", \"\",%d\n"
							   "\t\tP: \"FrontAxisSign\", \"int\", \"Integer\", \"\",%d\n"
							   "\t\tP: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n"
							   "\t\tP: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n"
							   "\t\tP: \"UnitScaleFactor\", \"double\", \"Number\", \"\",%d\n"
							   "\t}\n}\n",
			Up, UpSign, Front, FrontSign, UnitScaleFactor);
	}

	[[nodiscard]] bool LoadFbxText(const FString& Text, FMeshData& Out)
	{
		const FString Path = FPaths::CreateTempFilename(*FPaths::EngineIntermediateDir(), "leon_test_tri", ".fbx");
		const bool bLoaded = FFileHelper::SaveStringToFile(Text, *Path) && LoadStaticMeshFromFbx(Path, Out);
		IFileManager::Get().Delete(*Path);
		return bLoaded;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticMeshBuilderImportMatchesLegacyTest,
	"System.MeshUtilities.StaticMeshBuilder.ImportMatchesLegacy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStaticMeshBuilderImportMatchesLegacyTest::RunTest(const FString& Parameters)
{
	// Built from an OBJ (converted at import), a mesh is the one the legacy cook gave: imported in its own space,
	// tangents in the legacy basis, then converted as a version 1 .lmesh was when it loaded.
	const FString Dir = FPaths::EngineIntermediateDir();
	const FString UvPath = FPaths::CreateTempFilename(*Dir, "leon_test_uv", ".obj");
	if (!TestTrue("UV OBJ written", FFileHelper::SaveStringToFile(UvObj, *UvPath)))
	{
		return false;
	}
	const FString Sources[] = {
		FPaths::Combine(FPaths::EngineSourceDir(), "Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj"), UvPath};
	for (const FString& Source : Sources)
	{
		const FString Name = FPaths::GetCleanFilename(Source);
		FString Error;
		FMeshData Built;
		if (!TestTrue(*(Name + ": built"), FStaticMeshBuilder::BuildFromFile(Source, Built, Error)))
		{
			continue;
		}
		FMeshData Legacy = LoadObjSourceSpace(Source);
		ComputeTangents(Legacy, EMeshDataBasis::LegacyYUp);
		FLegacyCoordinateConversion::ConvertMeshData(Legacy);
		CheckSameMesh(*this, Name, Built, Legacy, 1.0e-4f);
		TestEqual(*(Name + ": submeshes"), Built.Submeshes.Num(), Legacy.Submeshes.Num());
		TestEqual(*(Name + ": one slot per material"), Built.MaterialSlotNames.Num(), Built.Materials.Num());
	}
	FString Error;
	FMeshData Unused;
	TestFalse("Not a mesh source", FStaticMeshBuilder::BuildFromFile(UvPath + TEXT(".txt"), Unused, Error));
	TestTrue("Explained", Error.Contains(TEXT("Not a mesh source")));
	IFileManager::Get().Delete(*UvPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFbxStaticMeshAxesAndUnitsTest, "System.MeshUtilities.FbxImport.AxesAndUnits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFbxStaticMeshAxesAndUnitsTest::RunTest(const FString& Parameters)
{
	// One floor triangle, three ways: Y up in metres (Maya's axes), Z up front -Y in centimetres (3ds Max's axes, the
	// RightHandedZUp frame itself) and with no GlobalSettings (taken as Y up; FBX's unit default is 1 cm). A
	// right-handed Y-up point (x, y, z) lands on (x, z, y) in the engine world, as a glTF would; the normal (up) on +Z.
	const FVector Expected[3] = {FVector(0.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f), FVector(0.0f, 100.0f, 0.0f)};
	struct FCase
	{
		const TCHAR* Name;
		FString Text;
	};
	const FCase Cases[] = {
		{"Y up, metres",
			MakeTriangleFbx(MakeFbxSettings(1, 1, 2, 1, 100), "0,0,0, 1,0,0, 0,0,1", "0,1,0, 0,1,0, 0,1,0")},
		{"Z up, centimetres",
			MakeTriangleFbx(MakeFbxSettings(2, 1, 1, -1, 1), "0,0,0, 100,0,0, 0,-100,0", "0,0,1, 0,0,1, 0,0,1")},
		{"No axes", MakeTriangleFbx("", "0,0,0, 100,0,0, 0,0,100", "0,1,0, 0,1,0, 0,1,0")},
	};
	for (const FCase& Case : Cases)
	{
		FMeshData Data;
		if (!TestTrue(*(FString(Case.Name) + ": loaded"), LoadFbxText(Case.Text, Data)) ||
			!TestEqual(*(FString(Case.Name) + ": corners"), Data.Vertices.Num(), 3))
		{
			continue;
		}
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const FVertex& Vertex = Data.Vertices[Corner];
			TestTrue(*FString::Printf("%s: corner %d at (%g, %g, %g)", Case.Name, Corner,
						 static_cast<double>(Vertex.Position.X), static_cast<double>(Vertex.Position.Y),
						 static_cast<double>(Vertex.Position.Z)),
				Vertex.Position.Equals(Expected[Corner], 1.0e-4f));
			TestTrue(*FString::Printf("%s: corner %d normal", Case.Name, Corner),
				Vertex.Normal.Equals(FVector(0.0f, 0.0f, 1.0f), 1.0e-6f));
		}
		TestTrue(*(FString(Case.Name) + ": indices kept"), Data.Indices == TArray<uint32>({0, 1, 2}));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
