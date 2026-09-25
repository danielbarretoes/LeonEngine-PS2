#include "AssetImportUtils.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Factories/FbxFactory.h"
#include "Factories/GLTFImportFactory.h"
#include "Factories/GLTFMapFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/SoundFactory.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "Sound/SoundWave.h"
#include "Tests/LeonEdTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// The LeonEd factories: each source format to its asset class, the import data every imported asset keeps, and the
// materials and textures a mesh import makes (MapFactoryTests.cpp has the map importer).

namespace
{

	constexpr EObjectFlags AssetFlags = RF_Public | RF_Standalone;

	/** Imports File with a factory of FactoryClass (Settings applied) as /LeonEdTest/<Name>. */
	UObject* ImportWith(UClass* FactoryClass, const FString& File, const TCHAR* Name,
		const TMap<FString, FString>& Settings = TMap<FString, FString>(), UFactory** OutFactory = nullptr)
	{
		UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		(void)Factory->ApplyImportSettings(Settings);
		if (OutFactory != nullptr)
		{
			*OutFactory = Factory;
		}
		UPackage* Package = CreatePackage(*(FString(LeonEdTest::Root) + Name));
		return UFactory::StaticImportObject(nullptr, Package, FName(Name), AssetFlags, File, Factory);
	}

	/** Mip 0's texels. */
	TArray<uint8> ReadTexels(const UTexture2D& Texture)
	{
		TArray<uint8> Texels;
		const FByteBulkData& BulkData = Texture.GetPlatformData().Mips[0].BulkData;
		Texels.Append(
			static_cast<const uint8*>(BulkData.LockReadOnly()), static_cast<int32>(BulkData.GetBulkDataSize()));
		BulkData.Unlock();
		return Texels;
	}

	/** A one-triangle ASCII FBX without GlobalSettings (read as Y up, centimetres). */
	const TCHAR* const TriangleFbx = "; FBX 7.4.0 project file\n"
									 "FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n}\n"
									 "Objects:  {\n"
									 "\tGeometry: 1001, \"Geometry::Tri\", \"Mesh\" {\n"
									 "\t\tVertices: *9 {\n\t\t\ta: 0,0,0, 100,0,0, 0,0,100\n\t\t}\n"
									 "\t\tPolygonVertexIndex: *3 {\n\t\t\ta: 0,1,-3\n\t\t}\n"
									 "\t\tGeometryVersion: 124\n"
									 "\t}\n"
									 "\tModel: 2001, \"Model::Tri\", \"Mesh\" {\n\t\tVersion: 232\n\t}\n"
									 "}\n"
									 "Connections:  {\n\tC: \"OO\",1001,2001\n\tC: \"OO\",2001,0\n}\n";

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdFactoryForFileTest, "System.LeonEd.Factories.FactoryForFile",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdFactoryForFileTest::RunTest(const FString& Parameters)
{
	// The factory of a file is found by its extension (and the class asked for); names get UE's prefixes.
	TestTrue("png", UFactory::FindFactoryClassForFile(TEXT("A.png")) == UTextureFactory::StaticClass());
	TestTrue("JPG", UFactory::FindFactoryClassForFile(TEXT("A.JPG")) == UTextureFactory::StaticClass());
	TestTrue("obj", UFactory::FindFactoryClassForFile(TEXT("A.obj")) == UFbxFactory::StaticClass());
	TestTrue("fbx", UFactory::FindFactoryClassForFile(TEXT("A.fbx")) == UFbxFactory::StaticClass());
	TestTrue("gltf", UFactory::FindFactoryClassForFile(TEXT("A.gltf")) == UGLTFImportFactory::StaticClass());
	TestTrue("glb", UFactory::FindFactoryClassForFile(TEXT("A.glb")) == UGLTFImportFactory::StaticClass());
	TestTrue("wav", UFactory::FindFactoryClassForFile(TEXT("A.wav")) == USoundFactory::StaticClass());
	TestTrue("glb as a map",
		UFactory::FindFactoryClassForFile(TEXT("A.glb"), UWorld::StaticClass()) == UGLTFMapFactory::StaticClass());
	TestNull("lmat", UFactory::FindFactoryClassForFile(TEXT("A.lmat")));
	TestNull("unknown", UFactory::FindFactoryClassForFile(TEXT("A.xyz")));
	TestNull("not for this class", UFactory::FindFactoryClassForFile(TEXT("A.png"), USoundWave::StaticClass()));

	TestEqual("SM_", FAssetImportUtils::MakeAssetName(UStaticMesh::StaticClass(), TEXT("Cube")), FString("SM_Cube"));
	TestEqual("kept", FAssetImportUtils::MakeAssetName(UTexture2D::StaticClass(), TEXT("T_Default_D")),
		FString("T_Default_D"));
	TestEqual("sanitized", FAssetImportUtils::MakeAssetName(USoundWave::StaticClass(), TEXT("ui click-1")),
		FString("S_ui_click_1"));
	TestEqual("M_", FAssetImportUtils::GetAssetPrefix(UMaterial::StaticClass()), FString("M_"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdTextureFactoryTest, "System.LeonEd.Factories.Texture",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdTextureFactoryTest::RunTest(const FString& Parameters)
{
	// An image becomes RGBA8 texels, bottom row first, sRGB unless it is a normal map (a _N name, or
	// ColorSpaceMode=Linear); the import data names the file relative to the source root, with its MD5.
	LeonEdTest::FScopedTestContent Content;
	const TArray<uint8> RGB = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
	const TArray<uint8> Bmp = LeonEdTest::MakeBmp(2, 2, RGB);
	const FString File = LeonEdTest::WriteSource(TEXT("Textures/Checker.bmp"), Bmp);

	UTexture2D* Texture = Cast<UTexture2D>(ImportWith(UTextureFactory::StaticClass(), File, TEXT("T_Checker")));
	if (!TestNotNull("Imported", Texture))
	{
		return false;
	}
	TestEqual("Width", Texture->GetSizeX(), 2);
	TestEqual("Format", static_cast<int32>(Texture->GetPixelFormat()), static_cast<int32>(PF_R8G8B8A8));
	const TArray<uint8> Texels = ReadTexels(*Texture);
	TestTrue("Bottom row first, RGBA",
		Texels.Num() == 16 && Texels[0] == 255 && Texels[1] == 0 && Texels[3] == 255 && Texels[4] == 0 &&
			Texels[5] == 255 && Texels[8] == 0 && Texels[10] == 255);
	TestEqual("sRGB", static_cast<int32>(Texture->SRGB), 1);
	const UAssetImportData* ImportData = Texture->AssetImportData;
	if (TestNotNull("Import data", ImportData))
	{
		TestTrue(
			"Its subobject", ImportData->GetOuter() == Texture && ImportData->GetName() == TEXT("AssetImportData"));
		TestEqual("Relative to the source root", ImportData->SourceData.SourceFiles[0].RelativeFilename,
			FString("SourceArt/Textures/Checker.bmp"));
		TestEqual("MD5", ImportData->GetFirstFileHash(), FMD5::HashBytes(Bmp.GetData(), Bmp.Num()));
		TestTrue("Resolves back", FPaths::IsSamePath(ImportData->GetFirstFilename(), File));
	}

	UTexture2D* Normal = Cast<UTexture2D>(ImportWith(UTextureFactory::StaticClass(), File, TEXT("T_Checker_N")));
	TestTrue("A _N texture is linear", Normal != nullptr && Normal->SRGB == 0);
	TMap<FString, FString> Linear;
	Linear.Add(TEXT("ColorSpaceMode"), TEXT("Linear"));
	UTexture2D* Forced = Cast<UTexture2D>(ImportWith(UTextureFactory::StaticClass(), File, TEXT("T_Forced"), Linear));
	if (TestTrue("ColorSpaceMode=Linear", Forced != nullptr && Forced->SRGB == 0))
	{
		TestEqual("The setting is recorded", Forced->AssetImportData->ImportSettings.FindRef(TEXT("ColorSpaceMode")),
			FString("Linear"));
	}

	const FString Bad = LeonEdTest::WriteSource(TEXT("Textures/Bad.png"), FString(TEXT("not an image")));
	AddExpectedError(TEXT("cannot decode"), 1);
	AddExpectedError(TEXT("failed to import"), 1);
	TestNull("A file that is not an image", ImportWith(UTextureFactory::StaticClass(), Bad, TEXT("T_Bad")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdSoundFactoryTest, "System.LeonEd.Factories.Sound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdSoundFactoryTest::RunTest(const FString& Parameters)
{
	// A PCM16 .wav keeps its samples, channels and rate; any other sample format is refused.
	LeonEdTest::FScopedTestContent Content;
	const TArray<int16> Samples = {0, 1000, -1000, 32767, -32768, 5};
	const FString File = LeonEdTest::WriteSource(TEXT("Audio/Beep.wav"), LeonEdTest::MakeWave(Samples, 2, 11025));
	const FString Bad = LeonEdTest::WriteSource(TEXT("Audio/Bad.wav"), LeonEdTest::MakeWave(Samples, 1, 8000, 8));

	USoundWave* Sound = Cast<USoundWave>(ImportWith(USoundFactory::StaticClass(), File, TEXT("S_Beep")));
	if (TestNotNull("Imported", Sound))
	{
		TestEqual("Channels", Sound->NumChannels, 2);
		TestEqual("Rate", Sound->SampleRate, 11025);
		TestEqual("Frames", Sound->GetNumFrames(), 3);
		TArray<int16> Loaded;
		Sound->GetPCMData(Loaded);
		TestTrue("Samples", Loaded == Samples);
		TestNotNull("Import data", Sound->AssetImportData);
	}
	AddExpectedError(TEXT("not 16-bit PCM"), 1);
	AddExpectedError(TEXT("failed to import"), 1);
	TestNull("An 8-bit file", ImportWith(USoundFactory::StaticClass(), Bad, TEXT("S_Bad")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdStaticMeshFactoryTest, "System.LeonEd.Factories.StaticMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdStaticMeshFactoryTest::RunTest(const FString& Parameters)
{
	// OBJ, FBX and glTF become static meshes in the engine world. A named source material becomes an M_ material next
	// to the mesh (its maps T_ textures), made once and kept by the slot on a reimport; an unnamed slot has none.
	LeonEdTest::FScopedTestContent Content;
	const FString Cube =
		FPaths::Combine(FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj"));
	UStaticMesh* CubeMesh = Cast<UStaticMesh>(ImportWith(UFbxFactory::StaticClass(), Cube, TEXT("SM_Cube")));
	if (TestNotNull("OBJ", CubeMesh))
	{
		TestEqual("Triangles", CubeMesh->GetNumTriangles(), 12);
		// Cube.obj spans [-1, 1] metres: 200 cm in the engine world.
		TestEqual("Centimetres", CubeMesh->GetBoundingBox().Max.Z - CubeMesh->GetBoundingBox().Min.Z, 200.0f, 1.0e-3f);
		TestEqual("One slot", CubeMesh->GetStaticMaterials().Num(), 1);
		TestNull("An unnamed slot has no material", CubeMesh->GetMaterial(0));
		TestNotNull("Body setup", CubeMesh->GetBodySetup());
		TestNotNull("Import data", CubeMesh->AssetImportData);
	}

	const TArray<uint8> RGB = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Crate_D.bmp"), LeonEdTest::MakeBmp(2, 2, RGB));
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Crate.mtl"),
		FString(TEXT("newmtl Wood\nKd 0.5 0.25 0.125\nKs 0 0 0\nNs 10\nd 1\nmap_Kd Crate_D.bmp\n")));
	const FString Crate = LeonEdTest::WriteSource(TEXT("Meshes/Crate.obj"),
		FString(TEXT("mtllib Crate.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvn 0 0 1\nusemtl Wood\n"
					 "f 1/1/1 2/2/1 3/3/1\n")));
	UFactory* Factory = nullptr;
	UStaticMesh* CrateMesh =
		Cast<UStaticMesh>(ImportWith(UFbxFactory::StaticClass(), Crate, TEXT("SM_Crate"), {}, &Factory));
	if (TestNotNull("OBJ with a material", CrateMesh))
	{
		TestEqual("The slot's name", CrateMesh->GetStaticMaterials()[0].MaterialSlotName, FName(TEXT("Wood")));
		const UMaterial* Wood = Cast<UMaterial>(CrateMesh->GetMaterial(0));
		if (TestNotNull("M_Wood", Wood))
		{
			TestEqual("Next to the mesh", Wood->GetPathName(), FString("/LeonEdTest/M_Wood.M_Wood"));
			TestTrue("Kd", Wood->BaseColor.Equals(FLinearColor(0.5f, 0.25f, 0.125f, 1.0f)));
			TestTrue("Its map",
				Wood->BaseColorMap != nullptr &&
					Wood->BaseColorMap->GetPathName() == TEXT("/LeonEdTest/T_Crate_D.T_Crate_D"));
		}
		TestEqual("Material and texture made", Factory->AdditionalImportedObjects.Num(), 2);

		// Reimported: the slot keeps its material, nothing new is made.
		UFactory* Again = nullptr;
		UObject* Reimported = ImportWith(UFbxFactory::StaticClass(), Crate, TEXT("SM_Crate"), {}, &Again);
		TestTrue("In place", Reimported == CrateMesh);
		TestTrue("Kept its material", CrateMesh->GetMaterial(0) == Wood);
		TestEqual("Nothing new", Again->AdditionalImportedObjects.Num(), 0);
	}

	const FString Fbx = LeonEdTest::WriteSource(TEXT("Meshes/Tri.fbx"), FString(TriangleFbx));
	UStaticMesh* Tri = Cast<UStaticMesh>(ImportWith(UFbxFactory::StaticClass(), Fbx, TEXT("SM_Tri")));
	if (TestNotNull("FBX", Tri))
	{
		TestEqual("One triangle", Tri->GetNumTriangles(), 1);
		// Y up in cm: (0, 0, 100) lands on Y = 100 cm.
		TestEqual("Converted", Tri->GetBoundingBox().Max.Y, 100.0f, 1.0e-3f);
	}

	// glTF: a triangle in an external buffer, with a named material.
	TArray<uint8> Buffer;
	const float Positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
	Buffer.Append(reinterpret_cast<const uint8*>(Positions), sizeof(Positions));
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Tri.bin"), Buffer);
	const FString Gltf = LeonEdTest::WriteSource(TEXT("Meshes/Tri.gltf"),
		FString(TEXT("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"Tri.bin\",\"byteLength\":36}],"
					 "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
					 "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
					 "\"min\":[0,0,0],\"max\":[1,1,0]}],"
					 "\"materials\":[{\"name\":\"Paint\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,0,1],"
					 "\"metallicFactor\":0.25,\"roughnessFactor\":0.5}}],"
					 "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}]}")));
	UStaticMesh* GltfMesh = Cast<UStaticMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Gltf, TEXT("SM_TriGltf")));
	if (TestNotNull("glTF", GltfMesh))
	{
		TestEqual("One triangle", GltfMesh->GetNumTriangles(), 1);
		// glTF is Y up in metres: (0, 1, 0) is 100 cm up.
		TestEqual("Converted", GltfMesh->GetBoundingBox().Max.Z, 100.0f, 1.0e-3f);
		const UMaterial* Paint = Cast<UMaterial>(GltfMesh->GetMaterial(0));
		TestTrue("M_Paint",
			Paint != nullptr && Paint->BaseColor.Equals(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)) &&
				FMath::IsNearlyEqual(Paint->Metallic, 0.25f));
	}

	// Skeletal meshes and animations need a skin and a skeleton.
	TMap<FString, FString> Skeletal;
	Skeletal.Add(TEXT("MeshTypeToImport"), TEXT("FBXIT_SkeletalMesh"));
	AddExpectedError(TEXT("no skinned mesh"), 2);
	AddExpectedError(TEXT("failed to import"), 2);
	TestNull("No skin", ImportWith(UFbxFactory::StaticClass(), Fbx, TEXT("SK_Tri"), Skeletal));
	TMap<FString, FString> Animation;
	Animation.Add(TEXT("MeshTypeToImport"), TEXT("FBXIT_Animation"));
	AddExpectedError(TEXT("needs a Skeleton"), 1);
	TestNull("No skeleton", ImportWith(UFbxFactory::StaticClass(), Fbx, TEXT("A_Tri"), Animation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMaterialFactoriesTest, "System.LeonEd.Factories.Materials",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMaterialFactoriesTest::RunTest(const FString& Parameters)
{
	// UMaterialFactoryNew makes a default material, without import data.
	LeonEdTest::FScopedTestContent Content;
	UMaterialFactoryNew* NewFactory = NewObject<UMaterialFactoryNew>();
	UMaterial* Fresh = Cast<UMaterial>(NewFactory->FactoryCreateNew(
		UMaterial::StaticClass(), CreatePackage(TEXT("/LeonEdTest/M_New")), TEXT("M_New"), AssetFlags, nullptr));
	if (TestNotNull("New material", Fresh))
	{
		const FMaterial Default;
		TestTrue("Default values", Fresh->GetRenderProxy().Albedo == Default.Albedo);
		TestNull("No import data", UFactory::GetAssetImportData(Fresh));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
