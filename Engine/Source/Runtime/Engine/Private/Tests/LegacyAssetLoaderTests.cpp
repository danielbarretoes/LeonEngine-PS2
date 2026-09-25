#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "LegacyAssetLoader.h"
#include "LeonMeshFormat.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "Sound/SoundWave.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

// The transitional legacy loader (FLegacyAssetLoader): the legacy files it still reads, and the garbage collection of
// what it made.

namespace
{

	/** Where the tests write their legacy files (deleted by each test). */
	FString GetLegacyTestDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/LegacyAssets/");
	}

	void AppendU16(TArray<uint8>& Bytes, uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 8) & 0xFF));
	}

	void AppendU32(TArray<uint8>& Bytes, uint32 Value)
	{
		AppendU16(Bytes, Value & 0xFFFF);
		AppendU16(Bytes, Value >> 16);
	}

	void AppendTag(TArray<uint8>& Bytes, const char* Tag)
	{
		Bytes.Append(reinterpret_cast<const uint8*>(Tag), 4);
	}

	/** A RIFF / WAVE file of Samples (interleaved) with BitsPerSample bits (16: PCM16; 8: an 8-bit file). */
	TArray<uint8> MakeWave(const TArray<int16>& Samples, int32 Channels, int32 Rate, int32 BitsPerSample = 16)
	{
		const int32 BytesPerSample = BitsPerSample / 8;
		const int32 DataSize = Samples.Num() * BytesPerSample;
		TArray<uint8> Bytes;
		AppendTag(Bytes, "RIFF");
		AppendU32(Bytes, static_cast<uint32>(36 + DataSize));
		AppendTag(Bytes, "WAVE");
		AppendTag(Bytes, "fmt ");
		AppendU32(Bytes, 16);
		AppendU16(Bytes, 1);
		AppendU16(Bytes, static_cast<uint32>(Channels));
		AppendU32(Bytes, static_cast<uint32>(Rate));
		AppendU32(Bytes, static_cast<uint32>(Rate * Channels * BytesPerSample));
		AppendU16(Bytes, static_cast<uint32>(Channels * BytesPerSample));
		AppendU16(Bytes, static_cast<uint32>(BitsPerSample));
		AppendTag(Bytes, "data");
		AppendU32(Bytes, static_cast<uint32>(DataSize));
		for (const int16 Sample : Samples)
		{
			if (BytesPerSample == 2)
			{
				AppendU16(Bytes, static_cast<uint16>(Sample));
			}
			else
			{
				Bytes.Add(static_cast<uint8>(Sample));
			}
		}
		return Bytes;
	}

	/** The engine config the defaults come from: GEngine's, or the class default object's. */
	const UEngine& GetEngineConfig()
	{
		return GEngine != nullptr ? *GEngine : *GetDefault<UEngine>();
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyAssetLoaderFilesTest, "System.Engine.LegacyAssets.LoadsLegacyFiles",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyAssetLoaderFilesTest::RunTest(const FString& Parameters)
{
	// A .lmat, a .lmesh, an image and a .wav become transient assets in their files' transient packages, cached by
	// path while they live; unreadable files give null with an error.
	const FString Dir = GetLegacyTestDir() + TEXT("Files/");
	const FString MaterialFile = Dir + TEXT("Materials/M_Test.lmat");
	const FString MeshFile = Dir + TEXT("Meshes/SM_Test.lmesh");
	const FString SoundFile = Dir + TEXT("Audio/S_Test.wav");
	const FString BadSoundFile = Dir + TEXT("Audio/S_Bad.wav");
	// The engine's texture source (an absolute map path is kept as it is).
	const FString EngineTexture = FPaths::EngineDir() + TEXT("SourceArt/EngineMaterials/T_Default_D.png");
	const FString Material =
		TEXT("[Info]\nName=M_Test\nShadingModel=Unlit\n[Parameters]\nBaseColor=0.25,0.5,0.75\nOpacity=0.5\n")
			TEXT("UVScale=2,3\nCastsShadows=false\n[Textures]\nBaseColorMap=") +
		EngineTexture + TEXT("\nNormalMap=bump\n");
	TestTrue("Material written", FFileHelper::SaveStringToFile(Material, *MaterialFile));
	TestTrue("Mesh written", SaveLeonMeshFile(MeshFile, MakeCube()));
	const TArray<int16> Samples = {0, 1000, -1000, 32767, -32768, 5};
	TestTrue("Sound written", FFileHelper::SaveArrayToFile(MakeWave(Samples, 2, 11025), *SoundFile));
	TestTrue("8-bit sound written", FFileHelper::SaveArrayToFile(MakeWave(Samples, 1, 8000, 8), *BadSoundFile));

	UMaterial* Loaded = FLegacyAssetLoader::LoadMaterial(MaterialFile);
	if (!TestNotNull("Material loaded", Loaded))
	{
		return false;
	}
	TestTrue("Transient", Loaded->HasAnyFlags(RF_Transient));
	TestTrue("In its file's package",
		Loaded->GetOutermost()->GetName() == FLegacyAssetLoader::GetLegacyPackageName(MaterialFile));
	TestTrue(
		"Under the legacy root", Loaded->GetOutermost()->GetName().StartsWith(FLegacyAssetLoader::LegacyPackageRoot));
	TestTrue("Named after the file", Loaded->GetName() == TEXT("M_Test"));
	TestTrue("Cached by path", FLegacyAssetLoader::LoadMaterial(MaterialFile) == Loaded);
	TestTrue("Unlit", Loaded->ShadingModel == MSM_Unlit);
	TestTrue("Base colour", Loaded->BaseColor.Equals(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f)));
	TestEqual("Opacity", Loaded->Opacity, 0.5f);
	TestTrue("UV scale", Loaded->UVScale == FVector2D(2.0f, 3.0f));
	TestFalse("No shadows", Loaded->bCastsShadows);
	if (TestNotNull("Base colour map", Loaded->BaseColorMap))
	{
		TestTrue("The texture file's asset", Loaded->BaseColorMap == FLegacyAssetLoader::LoadTexture(EngineTexture));
		TestTrue("Texels", Loaded->BaseColorMap->HasValidPlatformData());
		TestTrue("A file outside the content's package",
			Loaded->BaseColorMap->GetOutermost()->GetName().StartsWith(
				FString(FLegacyAssetLoader::LegacyPackageRoot) + TEXT("/External/")));
	}
	TestTrue("The bump map is the engine's",
		Loaded->NormalMap != nullptr &&
			Loaded->NormalMap->GetPathName() == GetEngineConfig().DefaultBumpNormalTextureName.ToString());

	UStaticMesh* Mesh = FLegacyAssetLoader::LoadStaticMesh(MeshFile);
	if (TestNotNull("Mesh loaded", Mesh))
	{
		TestEqual("Mesh triangles", Mesh->GetNumTriangles(), MakeCube().Indices.Num() / 3);
		TestNotNull("Mesh body setup", Mesh->GetBodySetup());
		TestEqual("One slot", Mesh->GetStaticMaterials().Num(), 1);
		const UMaterialInterface* Slot = Mesh->GetMaterial(0);
		if (TestNotNull("The slot's material", Slot))
		{
			const FMaterial Default;
			TestTrue("A plain slot material", Slot->GetRenderProxy().Albedo == Default.Albedo);
		}
		TestTrue("Mesh cached", FLegacyAssetLoader::LoadStaticMesh(MeshFile) == Mesh);
	}

	USoundWave* Sound = FLegacyAssetLoader::LoadSoundWave(SoundFile);
	if (TestNotNull("Sound loaded", Sound))
	{
		TestEqual("Channels", Sound->NumChannels, 2);
		TestEqual("Rate", Sound->SampleRate, 11025);
		TestEqual("Frames", Sound->GetNumFrames(), 3);
		TArray<int16> Loaded16;
		Sound->GetPCMData(Loaded16);
		TestTrue("Samples", Loaded16 == Samples);
	}

	AddExpectedError(TEXT("is not 16-bit PCM"), 1);
	TestNull("An 8-bit file is refused", FLegacyAssetLoader::LoadSoundWave(BadSoundFile));
	AddExpectedError(TEXT("cannot load texture"), 1);
	TestNull("A missing image", FLegacyAssetLoader::LoadTexture(Dir + TEXT("Missing.png")));
	AddExpectedError(TEXT("expected a .lmesh"), 1);
	TestNull("Not a .lmesh", FLegacyAssetLoader::LoadStaticMesh(MaterialFile));
	AddExpectedError(TEXT("cannot open"), 1);
	TestNull("A missing .lmat", FLegacyAssetLoader::LoadMaterial(Dir + TEXT("Missing.lmat")));

	IFileManager::Get().DeleteDirectory(*GetLegacyTestDir(), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyAssetLoaderGarbageTest, "System.Engine.LegacyAssets.CollectedWhenUnused",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyAssetLoaderGarbageTest::RunTest(const FString& Parameters)
{
	// Only their users keep legacy assets alive: an unused material is collected and the next load reads the file
	// again; a material a component uses stays.
	const FString File = GetLegacyTestDir() + TEXT("Garbage/M_Gone.lmat");
	TestTrue("Written", FFileHelper::SaveStringToFile(FString(TEXT("[Parameters]\nMetallic=0.5\n")), *File));

	TWeakObjectPtr<UMaterial> Unused = FLegacyAssetLoader::LoadMaterial(File);
	TestTrue("Loaded", Unused.IsValid());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("Collected without users", Unused.IsValid());

	{
		FScopedTestWorld TestWorld;
		AStaticMeshActor* Actor = TestWorld->SpawnActor<AStaticMeshActor>();
		UMaterial* Used = FLegacyAssetLoader::LoadMaterial(File);
		if (TestNotNull("Loaded again", Used))
		{
			TestEqual("Read the file again", Used->Metallic, 0.5f);
			Actor->GetStaticMeshComponent()->SetMaterial(0, Used);
			const TWeakObjectPtr<UMaterial> WeakUsed = Used;
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			TestTrue("Kept by the component", WeakUsed.IsValid());
			TestTrue("Still the cached one", FLegacyAssetLoader::LoadMaterial(File) == Used);
		}
	}
	IFileManager::Get().DeleteDirectory(*GetLegacyTestDir(), false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
