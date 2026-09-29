#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetImportUtils.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Factories/GLTFImportFactory.h"
#include "Factories/GLTFMapFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/PhysicalMaterialFactoryNew.h"
#include "Factories/SoundFactory.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
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
		const int32 Size = static_cast<int32>(BulkData.GetBulkDataSize());
		const uint8* const Data = static_cast<const uint8*>(BulkData.LockReadOnly());
		if (Size > 0)
		{
			Texels.Append(Data, Size);
		}
		BulkData.Unlock();
		return Texels;
	}

	/** A glTF test fixture (MeshUtilities' Tests/Fixtures, written by MakeSkinnedFixture.py). */
	FString FixturePath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures"), FileName));
	}

	/** Bytes as base64 (a glTF data URI). */
	FString Base64Encode(const TArray<uint8>& Bytes)
	{
		const ANSICHAR* const Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		FString Out;
		for (int32 Index = 0; Index < Bytes.Num(); Index += 3)
		{
			const int32 Count = FMath::Min(3, Bytes.Num() - Index);
			uint32 Group = uint32(Bytes[Index]) << 16;
			Group |= Count > 1 ? uint32(Bytes[Index + 1]) << 8 : 0u;
			Group |= Count > 2 ? uint32(Bytes[Index + 2]) : 0u;
			for (int32 Digit = 0; Digit < 4; ++Digit)
			{
				Out.AppendChar(Digit <= Count ? Digits[(Group >> (18 - (Digit * 6))) & 63] : '=');
			}
		}
		return Out;
	}

	/**
	 * A one-triangle .gltf, its buffer a data URI: (0, 0, 0), (1, 0, 0), (0, 1, 0) m with texture coordinates, and the
	 * given materials, textures and images (JSON arrays, or empty) with primitive material 0 when there are materials.
	 */
	FString MakeTriangleGltf(const FString& Materials, const FString& Images)
	{
		TArray<uint8> Buffer;
		const float Positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
		const float TexCoords[6] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
		Buffer.Append(reinterpret_cast<const uint8*>(Positions), sizeof(Positions));
		Buffer.Append(reinterpret_cast<const uint8*>(TexCoords), sizeof(TexCoords));
		FString Text = FString(TEXT("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":60,")
							   TEXT("\"uri\":\"data:application/octet-stream;base64,")) +
			Base64Encode(Buffer) +
			TEXT("\"}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,")
				TEXT("\"byteLength\":24}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,")
					TEXT("\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},{\"bufferView\":1,")
						TEXT("\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}],");
		const bool bMaterial = !Materials.IsEmpty();
		if (bMaterial)
		{
			Text += TEXT("\"materials\":") + Materials + TEXT(",");
		}
		if (!Images.IsEmpty())
		{
			Text += TEXT("\"textures\":[{\"source\":0}],\"images\":") + Images + TEXT(",");
		}
		Text += FString(TEXT("\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1}")) +
			(bMaterial ? TEXT(",\"material\":0") : TEXT("")) + TEXT("}]}]}");
		return Text;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdFactoryForFileTest, "System.LeonEd.Factories.FactoryForFile",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdFactoryForFileTest::RunTest(const FString& Parameters)
{
	// The factory of a file is found by its extension (and the class asked for); names get UE's prefixes.
	TestTrue("png", UFactory::FindFactoryClassForFile(TEXT("A.png")) == UTextureFactory::StaticClass());
	TestTrue("JPG", UFactory::FindFactoryClassForFile(TEXT("A.JPG")) == UTextureFactory::StaticClass());
	TestNull("obj: glTF is the only mesh format", UFactory::FindFactoryClassForFile(TEXT("A.obj")));
	TestNull("fbx: glTF is the only mesh format", UFactory::FindFactoryClassForFile(TEXT("A.fbx")));
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
	// A PCM16 .wav keeps its samples, channels and rate as the sound's source, with the settings its SPU2 ADPCM is made
	// with (22 050 Hz, a one-shot, unless the import says otherwise); any other sample format is refused.
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
		TestTrue("The default settings",
			Sound->CompressionSampleRate == 22050 && !Sound->bLooping && Sound->Priority == 1.0f);
		TestTrue("Its ADPCM: mono at the source's lower rate, one block",
			Sound->CacheCompressedData() && Sound->GetCompressedSampleRate() == 11025 &&
				Sound->GetCompressedDataSize() == FSpuAdpcm::BytesPerBlock);
	}
	TMap<FString, FString> Settings;
	Settings.Add(TEXT("CompressionSampleRate"), TEXT("8000"));
	Settings.Add(TEXT("bLooping"), TEXT("True"));
	Settings.Add(TEXT("Priority"), TEXT("2.5"));
	USoundWave* Loop = Cast<USoundWave>(ImportWith(USoundFactory::StaticClass(), File, TEXT("S_Loop"), Settings));
	if (TestNotNull("Imported with settings", Loop))
	{
		TestTrue("The settings", Loop->CompressionSampleRate == 8000 && Loop->bLooping && Loop->Priority == 2.5f);
		TestTrue("A looping ADPCM at 8 kHz",
			Loop->CacheCompressedData() && Loop->GetCompressedSampleRate() == 8000 &&
				Loop->LockCompressedData().IsLooping());
		Loop->UnlockCompressedData();
		TestEqual("Recorded", Loop->AssetImportData->ImportSettings.FindRef(TEXT("bLooping")), FString(TEXT("True")));
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
	// glTF becomes static meshes in the engine world. A named source material becomes an M_ material next to the mesh,
	// its base colour image a T_ texture (an external file imported on its own, an embedded one made from its bytes),
	// made once and kept by the slot on a reimport; an unnamed slot has none.
	LeonEdTest::FScopedTestContent Content;
	UStaticMesh* CubeMesh = Cast<UStaticMesh>(
		ImportWith(UGLTFImportFactory::StaticClass(), FixturePath(TEXT("Cube.glb")), TEXT("SM_Cube")));
	if (TestNotNull("Cube.glb", CubeMesh))
	{
		TestEqual("Triangles", CubeMesh->GetNumTriangles(), 12);
		// Cube.glb spans [-1, 1] metres: 200 cm in the engine world.
		TestEqual("Centimetres", CubeMesh->GetBoundingBox().Max.Z - CubeMesh->GetBoundingBox().Min.Z, 200.0f, 1.0e-3f);
		TestEqual("One slot", CubeMesh->GetStaticMaterials().Num(), 1);
		TestNull("An unnamed slot has no material", CubeMesh->GetMaterial(0));
		TestNotNull("Body setup", CubeMesh->GetBodySetup());
		TestNotNull("Import data", CubeMesh->AssetImportData);
	}

	// An external image next to the .gltf.
	const TArray<uint8> RGB = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Crate_D.bmp"), LeonEdTest::MakeBmp(2, 2, RGB));
	const FString Crate = LeonEdTest::WriteSource(TEXT("Meshes/Crate.gltf"),
		MakeTriangleGltf(TEXT("[{\"name\":\"Wood\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.25,0.125,1],")
							 TEXT("\"baseColorTexture\":{\"index\":0}}}]"),
			TEXT("[{\"uri\":\"Crate_D.bmp\"}]")));
	UFactory* Factory = nullptr;
	UStaticMesh* CrateMesh =
		Cast<UStaticMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Crate, TEXT("SM_Crate"), {}, &Factory));
	if (TestNotNull("glTF with a material", CrateMesh))
	{
		TestEqual("The slot's name", CrateMesh->GetStaticMaterials()[0].MaterialSlotName, FName(TEXT("Wood")));
		const UMaterial* Wood = Cast<UMaterial>(CrateMesh->GetMaterial(0));
		if (TestNotNull("M_Wood", Wood))
		{
			TestEqual("Next to the mesh", Wood->GetPathName(), FString("/LeonEdTest/M_Wood.M_Wood"));
			TestTrue("Its colour", Wood->BaseColor.Equals(FLinearColor(0.5f, 0.25f, 0.125f, 1.0f)));
			TestTrue("Its map",
				Wood->BaseColorMap != nullptr &&
					Wood->BaseColorMap->GetPathName() == TEXT("/LeonEdTest/T_Crate_D.T_Crate_D"));
			TestNotNull("An external image has its own import data",
				Wood->BaseColorMap != nullptr ? Wood->BaseColorMap->AssetImportData : nullptr);
		}
		TestEqual("Material and texture made", Factory->AdditionalImportedObjects.Num(), 2);

		// Reimported: the slot keeps its material, nothing new is made.
		UFactory* Again = nullptr;
		UObject* Reimported = ImportWith(UGLTFImportFactory::StaticClass(), Crate, TEXT("SM_Crate"), {}, &Again);
		TestTrue("In place", Reimported == CrateMesh);
		TestTrue("Kept its material", CrateMesh->GetMaterial(0) == Wood);
		TestEqual("Nothing new", Again->AdditionalImportedObjects.Num(), 0);
	}

	// An image embedded as a data URI (a BMP: stb_image reads it like a PNG).
	const FString Painted = LeonEdTest::WriteSource(TEXT("Meshes/Painted.gltf"),
		MakeTriangleGltf(TEXT("[{\"name\":\"Paint\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,0,1],")
							 TEXT("\"baseColorTexture\":{\"index\":0}}}]"),
			TEXT("[{\"name\":\"Paint_D\",\"uri\":\"data:image/bmp;base64,") +
				Base64Encode(LeonEdTest::MakeBmp(2, 2, RGB)) + TEXT("\"}]")));
	UStaticMesh* GltfMesh =
		Cast<UStaticMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Painted, TEXT("SM_Painted")));
	if (TestNotNull("glTF", GltfMesh))
	{
		TestEqual("One triangle", GltfMesh->GetNumTriangles(), 1);
		// glTF is Y up in metres: (0, 1, 0) is 100 cm up.
		TestEqual("Converted", GltfMesh->GetBoundingBox().Max.Z, 100.0f, 1.0e-3f);
		const UMaterial* Paint = Cast<UMaterial>(GltfMesh->GetMaterial(0));
		TestTrue("M_Paint", Paint != nullptr && Paint->BaseColor.Equals(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
		const UTexture2D* Embedded = Paint != nullptr ? Paint->BaseColorMap : nullptr;
		if (TestNotNull("T_Paint_D from the data URI", Embedded))
		{
			TestEqual("Its name", Embedded->GetPathName(), FString("/LeonEdTest/T_Paint_D.T_Paint_D"));
			TestEqual("Its size", Embedded->GetSizeX(), 2);
			TestTrue("Its texels", ReadTexels(*Embedded).Num() == 16 && ReadTexels(*Embedded)[0] == 10);
			TestNull("No import data: the mesh's import makes it", Embedded->AssetImportData);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdGltfSkeletalTest, "System.LeonEd.Factories.GltfSkeletalMeshAndAnimations",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdGltfSkeletalTest::RunTest(const FString& Parameters)
{
	// SkinnedArm.glb as a skeletal mesh: SK_ on a new SKEL_ with the file's bones and socket, its slot's material with
	// the texture embedded in the .glb; as animations: an A_ per glTF animation on that skeleton, each recording its
	// animation for the reimport. A skeleton with other bones is refused.
	LeonEdTest::FScopedTestContent Content;
	const FString Arm = FixturePath(TEXT("SkinnedArm.glb"));
	TMap<FString, FString> AsSkeletal;
	AsSkeletal.Add(TEXT("ImportType"), TEXT("SkeletalMesh"));
	UFactory* Factory = nullptr;
	USkeletalMesh* Mesh =
		Cast<USkeletalMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("SK_Arm"), AsSkeletal, &Factory));
	if (!TestNotNull("SK_Arm", Mesh))
	{
		return false;
	}
	USkeleton* Skeleton = Mesh->Skeleton;
	if (!TestNotNull("Its skeleton", Skeleton))
	{
		return false;
	}
	TestEqual("SKEL_ next to it", Skeleton->GetPathName(), FString("/LeonEdTest/SKEL_Arm.SKEL_Arm"));
	TestEqual("Three bones", Skeleton->GetReferenceSkeleton().GetNum(), 3);
	TestTrue("Skinned render data", Mesh->GetRenderData().IsSkinned() && Mesh->GetNumTriangles() == 32);
	const USkeletalMeshSocket* Grip = Skeleton->FindSocket(TEXT("Grip"));
	TestTrue("The socket on the hand", Grip != nullptr && Grip->BoneName == FName(TEXT("Hand")));
	const UMaterial* Skin = Mesh->Materials.Num() == 1 ? Cast<UMaterial>(Mesh->GetMaterial(0)) : nullptr;
	if (TestNotNull("M_ArmSkin", Skin))
	{
		const UTexture2D* Texture = Skin->BaseColorMap;
		TestTrue("The embedded image",
			Texture != nullptr && Texture->GetSizeX() == 8 && Texture->GetSizeY() == 8 &&
				Texture->GetPathName() == TEXT("/LeonEdTest/T_ArmSkin_D.T_ArmSkin_D"));
	}
	TestEqual("Skeleton, material and texture", Factory->AdditionalImportedObjects.Num(), 3);
	TestEqual("The type recorded", Mesh->AssetImportData->ImportSettings.FindRef(TEXT("ImportType")),
		FString("SkeletalMesh"));

	// The animations, on the skeleton.
	TMap<FString, FString> AsAnimations;
	AsAnimations.Add(TEXT("ImportType"), TEXT("Animation"));
	AsAnimations.Add(TEXT("Skeleton"), Skeleton->GetPathName());
	UAnimSequence* Wave = Cast<UAnimSequence>(
		ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("A_SkinnedArm"), AsAnimations, &Factory));
	if (!TestNotNull("The first animation", Wave))
	{
		return false;
	}
	TestEqual("Named after it", Wave->GetPathName(), FString("/LeonEdTest/A_Wave.A_Wave"));
	TestTrue("On the skeleton", Wave->GetSkeleton() == Skeleton);
	TestEqual("31 frames", Wave->GetNumberOfFrames(), 31);
	TestEqual("Its animation recorded", Wave->AssetImportData->ImportSettings.FindRef(TEXT("AnimationName")),
		FString("Wave"));
	UAnimSequence* Grip2 = Factory->AdditionalImportedObjects.Num() == 1
		? Cast<UAnimSequence>(Factory->AdditionalImportedObjects[0])
		: nullptr;
	TestTrue("The second one", Grip2 != nullptr && Grip2->GetPathName() == TEXT("/LeonEdTest/A_Grip.A_Grip"));
	if (Grip2 != nullptr)
	{
		TArray<UObject*> Reimported;
		TestTrue("Reimported alone", FReimportManager::Instance()->Reimport(Grip2, &Reimported));
		TestTrue("In place, the same clip",
			Grip2->GetNumberOfFrames() == 31 && Grip2->GetCompressedData().GetNumTracks() == 3 &&
				Grip2->GetPathName() == TEXT("/LeonEdTest/A_Grip.A_Grip"));
		// The loop flag of the glTF extras (N27): Grip is a one-shot, Wave (no flag) loops.
		TestFalse("Grip does not loop", Grip2->bLoop);
		TestTrue("Wave loops", Wave->bLoop);
	}

	// Two meshes on one skeleton (the teams' bodies, N27): the first names the skeleton it makes, the second is given
	// it.
	TMap<FString, FString> Named = AsSkeletal;
	Named.Add(TEXT("NewSkeletonName"), TEXT("SKEL_Shared"));
	USkeletalMesh* First =
		Cast<USkeletalMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("SK_Arm_CT"), Named));
	TestTrue("The skeleton it names",
		First != nullptr && First->Skeleton != nullptr &&
			First->Skeleton->GetPathName() == TEXT("/LeonEdTest/SKEL_Shared.SKEL_Shared"));
	TMap<FString, FString> Shared = AsSkeletal;
	Shared.Add(TEXT("Skeleton"), TEXT("/LeonEdTest/SKEL_Shared.SKEL_Shared"));
	USkeletalMesh* Second =
		Cast<USkeletalMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("SK_Arm_T"), Shared));
	TestTrue("The second mesh on it", Second != nullptr && First != nullptr && Second->Skeleton == First->Skeleton);

	// Another skeleton's bones, and no skeleton at all.
	USkeleton* Other =
		NewObject<USkeleton>(CreatePackage(TEXT("/LeonEdTest/SKEL_Other")), TEXT("SKEL_Other"), AssetFlags);
	FReferenceSkeleton OneBone;
	OneBone.BoneNames = {FName(TEXT("Root"))};
	OneBone.ParentIndices = {INDEX_NONE};
	OneBone.RefBonePose = {FTransform::Identity};
	OneBone.InverseBindPose = {FMatrix::Identity};
	Other->SetReferenceSkeleton(OneBone);
	TMap<FString, FString> OnOther;
	OnOther.Add(TEXT("ImportType"), TEXT("Animation"));
	OnOther.Add(TEXT("Skeleton"), Other->GetPathName());
	AddExpectedError(TEXT("do not match"), 2);
	AddExpectedError(TEXT("failed to import"), 3);
	TestNull(
		"Another skeleton's animations", ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("A_Other"), OnOther));
	TMap<FString, FString> MeshOnOther;
	MeshOnOther.Add(TEXT("ImportType"), TEXT("SkeletalMesh"));
	MeshOnOther.Add(TEXT("Skeleton"), Other->GetPathName());
	TestNull("A mesh on another skeleton",
		ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("SK_Other"), MeshOnOther));
	TMap<FString, FString> NoSkeleton;
	NoSkeleton.Add(TEXT("ImportType"), TEXT("Animation"));
	AddExpectedError(TEXT("needs a Skeleton"), 1);
	TestNull("No skeleton", ImportWith(UGLTFImportFactory::StaticClass(), Arm, TEXT("A_None"), NoSkeleton));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdGltfSocketsTest, "System.LeonEd.Factories.GltfSockets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdGltfSocketsTest::RunTest(const FString& Parameters)
{
	// A glTF node named SOCKET_<Name> under the mesh's node becomes a socket of the static mesh (UE's FBX convention),
	// placed in the mesh's space in the engine's axes and centimetres; a reimport keeps the socket object.
	LeonEdTest::FScopedTestContent Content;
	TArray<uint8> Buffer;
	const float Positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
	Buffer.Append(reinterpret_cast<const uint8*>(Positions), sizeof(Positions));
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Gun.bin"), Buffer);
	const FString Gltf = LeonEdTest::WriteSource(TEXT("Meshes/Gun.gltf"),
		FString(TEXT("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"Gun.bin\",\"byteLength\":36}],"
					 "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
					 "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
					 "\"min\":[0,0,0],\"max\":[1,1,0]}],"
					 "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
					 "\"nodes\":[{\"name\":\"Gun\",\"mesh\":0,\"children\":[1]},"
					 "{\"name\":\"SOCKET_Muzzle\",\"translation\":[0.5,0.1,0.0]}],"
					 "\"scenes\":[{\"nodes\":[0]}],\"scene\":0}")));
	UStaticMesh* Gun = Cast<UStaticMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Gltf, TEXT("SM_Gun")));
	if (!TestNotNull("glTF", Gun))
	{
		return false;
	}
	TestEqual("One socket", Gun->Sockets.Num(), 1);
	UStaticMeshSocket* Muzzle = Gun->FindSocket(TEXT("Muzzle"));
	if (!TestNotNull("Muzzle", Muzzle))
	{
		return false;
	}
	// glTF (x, y, z) in metres is the engine's (x, z, y) in centimetres.
	TestTrue("Its place", Muzzle->RelativeLocation.Equals(FVector(50.0f, 0.0f, 10.0f), 1.0e-3f));
	TestTrue("An inner object", Muzzle->GetOuter() == Gun);
	UObject* Reimported = ImportWith(UGLTFImportFactory::StaticClass(), Gltf, TEXT("SM_Gun"));
	TestTrue("In place", Reimported == Gun);
	TestTrue("The same socket", Gun->Sockets.Num() == 1 && Gun->Sockets[0] == Muzzle);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdPhysicalMaterialsTest, "System.LeonEd.Factories.PhysicalMaterials",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdPhysicalMaterialsTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N30f: UPhysicalMaterialFactoryNew makes a PM_ of a surface type (a configured name, an enumerator or
	// Default); a glTF material's extras name its physical material (`physMaterial`), which the import sets on the M_
	// it makes and, the source deciding, on the one it finds again; a physical material that does not exist is an
	// error and changes nothing.
	LeonEdTest::FScopedTestContent Content;
	auto MakePhysicalMaterial = [](const TCHAR* Name, const TCHAR* Surface) -> UPhysicalMaterial*
	{
		UPhysicalMaterialFactoryNew* Factory = NewObject<UPhysicalMaterialFactoryNew>();
		Factory->SurfaceType = Surface;
		return Cast<UPhysicalMaterial>(Factory->FactoryCreateNew(UPhysicalMaterial::StaticClass(),
			CreatePackage(*(FString(LeonEdTest::Root) + Name)), FName(Name), AssetFlags, nullptr));
	};
	UPhysicalMaterial* Wood = MakePhysicalMaterial(TEXT("PM_Wood"), TEXT("SurfaceType4"));
	UPhysicalMaterial* Metal = MakePhysicalMaterial(TEXT("PM_Metal"), TEXT("SurfaceType3"));
	if (!TestTrue("Made", Wood != nullptr && Metal != nullptr))
	{
		return false;
	}
	TestEqual("Its surface", Wood->SurfaceType.GetValue(), SurfaceType4);
	UPhysicalMaterial* Plain = MakePhysicalMaterial(TEXT("PM_Plain"), TEXT("Default"));
	TestTrue("Default", Plain != nullptr && Plain->SurfaceType.GetValue() == SurfaceType_Default);
	AddExpectedError(TEXT("is no surface type"), 1);
	TestNull("An unknown surface", MakePhysicalMaterial(TEXT("PM_Lava"), TEXT("Lava")));

	auto Source = [](const TCHAR* PhysMaterial)
	{
		return MakeTriangleGltf(FString(TEXT("[{\"name\":\"Plank\",\"extras\":{\"physMaterial\":\"")) + PhysMaterial +
				TEXT("\"},\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.3,0.1,1]}}]"),
			FString());
	};
	const FString Plank = LeonEdTest::WriteSource(TEXT("Meshes/Plank.gltf"), Source(TEXT("/LeonEdTest/PM_Wood")));
	UStaticMesh* Mesh = Cast<UStaticMesh>(ImportWith(UGLTFImportFactory::StaticClass(), Plank, TEXT("SM_Plank")));
	const UMaterial* Material = Mesh != nullptr ? Cast<UMaterial>(Mesh->GetMaterial(0)) : nullptr;
	if (!TestNotNull("M_Plank", Material))
	{
		return false;
	}
	TestTrue("The extras' physical material", Material->PhysMaterial == Wood);

	// The source names another (an object path this time): the material the import made follows it.
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Plank.gltf"), Source(TEXT("/LeonEdTest/PM_Metal.PM_Metal")));
	UFactory* Factory = nullptr;
	TestTrue(
		"Reimported", ImportWith(UGLTFImportFactory::StaticClass(), Plank, TEXT("SM_Plank"), {}, &Factory) == Mesh);
	TestTrue("The same material, now metal", Mesh->GetMaterial(0) == Material && Material->PhysMaterial == Metal);
	TestTrue("To be saved", Factory != nullptr && Factory->AdditionalImportedObjects.Contains(Material));

	// One that does not exist: an error, the material keeps its own.
	(void)LeonEdTest::WriteSource(TEXT("Meshes/Plank.gltf"), Source(TEXT("/LeonEdTest/PM_Missing")));
	AddExpectedError(TEXT("which does not exist"), 1);
	(void)ImportWith(UGLTFImportFactory::StaticClass(), Plank, TEXT("SM_Plank"));
	TestTrue("Still metal", Material->PhysMaterial == Metal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdGLTFStaticMeshLODsTest, "System.LeonEd.GLTFImport.StaticMeshLODs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdGLTFStaticMeshLODsTest::RunTest(const FString& Parameters)
{
	// N15: ImportList.ini's `LODs=<share>@<size>,...` makes a static mesh's LODs after LOD 0 (meshoptimizer's
	// simplification at import, saved with the mesh); a bad entry fails the import; the same source gives the same
	// bytes.
	TArray<FStaticMeshSourceModel> Models;
	FString Error;
	TestTrue("Two LODs", UGLTFImportFactory::ParseLODs(TEXT("0.5@0.3, 0.25@0.1"), Models, Error));
	TestTrue("LOD 0 and two",
		Models.Num() == 3 && Models[0].ScreenSize == 1.0f && Models[1].ReductionSettings.PercentTriangles == 0.5f &&
			Models[2].ScreenSize == 0.1f);
	TestTrue("None", UGLTFImportFactory::ParseLODs(TEXT(""), Models, Error) && Models.Num() == 0);
	TestFalse("Not <share>@<size>", UGLTFImportFactory::ParseLODs(TEXT("0.5"), Models, Error));
	TestFalse("Sizes going up", UGLTFImportFactory::ParseLODs(TEXT("0.5@0.1,0.25@0.3"), Models, Error));
	TestFalse("A share past 1", UGLTFImportFactory::ParseLODs(TEXT("1.5@0.3"), Models, Error));

	LeonEdTest::FScopedTestContent Content;
	TMap<FString, FString> Settings;
	Settings.Add(TEXT("LODs"), TEXT("0.5@0.3"));
	const UStaticMesh* Mesh = Cast<UStaticMesh>(
		ImportWith(UGLTFImportFactory::StaticClass(), FixturePath(TEXT("Cube.glb")), TEXT("SM_CubeLODs"), Settings));
	if (!TestNotNull("Imported with LODs", Mesh))
	{
		return false;
	}
	TestEqual("Two LODs", Mesh->GetNumLODs(), 2);
	TestEqual("LOD 1's screen size", Mesh->GetLODScreenSize(1), 0.3f);
	TestTrue("LOD 1 no bigger than LOD 0",
		Mesh->GetLODResources(1).GetNumTriangles() <= Mesh->GetLODResources(0).GetNumTriangles());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
