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

// The transitional legacy loader (FLegacyAssetLoader), the engine's default assets from the config, and the
// garbage-collection safety of the assets the components and the scene proxies use.

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
	const FString Material =
		TEXT("[Info]\nName=M_Test\nShadingModel=Unlit\n[Parameters]\nBaseColor=0.25,0.5,0.75\nOpacity=0.5\n") TEXT(
			"UVScale=2,3\nCastsShadows=false\n[Textures]\nBaseColorMap=Textures/T_Default_D.png\nNormalMap=bump\n");
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
	const FString EngineTexture = FPaths::EngineContentDir() + TEXT("Textures/T_Default_D.png");
	if (TestNotNull("Base colour map", Loaded->BaseColorMap))
	{
		TestTrue("The texture file's asset", Loaded->BaseColorMap == FLegacyAssetLoader::LoadTexture(EngineTexture));
		TestTrue("Texels", Loaded->BaseColorMap->HasValidPlatformData());
		TestTrue("An engine content file's package",
			Loaded->BaseColorMap->GetOutermost()->GetName().StartsWith(
				FString(FLegacyAssetLoader::LegacyPackageRoot) + TEXT("/Engine/")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineDefaultAssetsTest, "System.Engine.LegacyAssets.EngineDefaultsFromConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineDefaultAssetsTest::RunTest(const FString& Parameters)
{
	// BaseEngine.ini names the default assets on UEngine; the loader makes them once, at those paths and in the root
	// set, where LoadObject finds them like any other asset; the basic shapes live at /Engine/BasicShapes.
	const UEngine& Engine = GetEngineConfig();
	TestEqual("DefaultMaterialName from the config", Engine.DefaultMaterialName.ToString(),
		FString("/Engine/EngineMaterials/M_Default.M_Default"));
	TestEqual("DefaultTextureName from the config", Engine.DefaultTextureName.ToString(),
		FString("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	TestEqual("DefaultBumpNormalTextureName from the config", Engine.DefaultBumpNormalTextureName.ToString(),
		FString("/Engine/EngineMaterials/T_Default_Bump_N.T_Default_Bump_N"));

	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull("Default material", DefaultMaterial))
	{
		return false;
	}
	TestEqual("At its config path", DefaultMaterial->GetPathName(), Engine.DefaultMaterialName.ToString());
	TestTrue("Rooted", DefaultMaterial->IsRooted());
	TestFalse("Not transient", DefaultMaterial->HasAnyFlags(RF_Transient));
	TestTrue("Made once", UMaterial::GetDefaultMaterial(MD_Surface) == DefaultMaterial);
	TestTrue("LoadObject finds it",
		LoadObject<UMaterial>(nullptr, *Engine.DefaultMaterialName.ToString()) == DefaultMaterial);
	TestTrue("Resolved by its soft path", Engine.DefaultMaterialName.ResolveObject() == DefaultMaterial);
	// M_Default.lmat: white, 8 shininess, the engine's T_Default_D.png.
	TestTrue("M_Default's base colour", DefaultMaterial->BaseColor.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	TestEqual("M_Default's shininess", DefaultMaterial->Shininess, 8.0f);
	TestNotNull("M_Default's map", DefaultMaterial->BaseColorMap);

	UTexture2D* Checker = FLegacyAssetLoader::LoadEngineObject<UTexture2D>(Engine.DefaultTextureName);
	if (TestNotNull("DefaultTexture", Checker))
	{
		TestEqual("Checker size", Checker->GetSizeX(), 64);
		TestTrue("Rooted checker", Checker->IsRooted());
		TestTrue("Found by path",
			FindObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")) == Checker);
	}
	UTexture2D* Bump = FLegacyAssetLoader::LoadEngineObject<UTexture2D>(Engine.DefaultBumpNormalTextureName);
	if (TestNotNull("Bump map", Bump))
	{
		TestEqual("Bump size", Bump->GetSizeX(), 256);
		TestEqual("A normal map is not sRGB", static_cast<int32>(Bump->SRGB), 0);
	}
	TestNull("The wrong class",
		FLegacyAssetLoader::LoadEngineObject<UMaterial>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))));
	TestNull("No source",
		FLegacyAssetLoader::LoadEngineObject<UMaterial>(
			FSoftObjectPath(TEXT("/Engine/EngineMaterials/M_None.M_None"))));

	const UStaticMesh* Cube =
		FLegacyAssetLoader::LoadEngineObject<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	const UStaticMesh* Plane =
		FLegacyAssetLoader::LoadEngineObject<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (TestNotNull("Cube", Cube) && TestNotNull("Plane", Plane))
	{
		TestEqual("Cube triangles", Cube->GetNumTriangles(), 12);
		TestEqual("Cube size (cm)", Cube->GetBoundingBox().Max.X - Cube->GetBoundingBox().Min.X, 100.0f, 1.0e-3f);
		TestEqual("The shapes have no slots", Cube->GetStaticMaterials().Num(), 0);
		TestEqual("Plane size (cm)", Plane->GetBoundingBox().Max.Y - Plane->GetBoundingBox().Min.Y, 100.0f, 1.0e-3f);
	}
	UStaticMesh* Sphere = FLegacyAssetLoader::GetSphereMesh(24, 16);
	UStaticMesh* CoarseSphere = FLegacyAssetLoader::GetSphereMesh(8, 6);
	if (TestNotNull("Sphere", Sphere) && TestNotNull("Coarse sphere", CoarseSphere))
	{
		TestEqual("The default sphere", Sphere->GetPathName(), FString("/Engine/BasicShapes/Sphere.Sphere"));
		TestTrue(
			"Another tessellation is another mesh", CoarseSphere != Sphere && CoarseSphere->HasAnyFlags(RF_Transient));
		TestTrue("Coarser", CoarseSphere->GetNumTriangles() < Sphere->GetNumTriangles());
		TestTrue("Cached per tessellation", FLegacyAssetLoader::GetSphereMesh(8, 6) == CoarseSphere);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenePrimitiveAssetsSurviveTest, "System.Engine.LegacyAssets.ProxiesKeepTheirAssets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FScenePrimitiveAssetsSurviveTest::RunTest(const FString& Parameters)
{
	// A proxy never outlives the assets it draws: the scene reports them to the garbage collector, even when the
	// component let go of them without recreating its proxy, and even pending kill; once the proxy is gone they go.
	// A slot without a material draws with the engine's default material.
	FScopedTestWorld TestWorld;
	if (!TestNotNull("The world has a scene", TestWorld->Scene))
	{
		return false;
	}
	UStaticMeshComponent& Component = *TestWorld->SpawnActor<AStaticMeshActor>()->GetStaticMeshComponent();
	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	(void)Mesh->BuildFromMeshData(MakeCube());
	UTexture2D* Texture = UTexture2D::CreateTransient(2, 2);
	UMaterial* Material = NewObject<UMaterial>();
	Material->BaseColorMap = Texture;
	(void)Component.SetStaticMesh(Mesh);
	Component.SetMaterial(0, Material);
	const FStaticMeshSceneProxy* Proxy = static_cast<const FStaticMeshSceneProxy*>(Component.SceneProxy);
	if (!TestNotNull("A proxy", Proxy))
	{
		return false;
	}
	TestTrue("The proxy's map", Proxy->GetSectionMaterial(0).AlbedoMap == Texture);

	const TWeakObjectPtr<UStaticMesh> WeakMesh = Mesh;
	const TWeakObjectPtr<UTexture2D> WeakTexture = Texture;
	const TWeakObjectPtr<UMaterial> WeakMaterial = Material;
	// Let go of everything behind the proxy's back (no MarkRenderStateDirty).
	Component.StaticMesh = nullptr;
	Component.OverrideMaterials.Empty();
	Material->BaseColorMap = nullptr;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("The mesh stays while the proxy draws it", WeakMesh.IsValid());
	TestTrue("The texture stays while the proxy draws it", WeakTexture.IsValid());
	TestFalse("The material itself is not drawn", WeakMaterial.IsValid());

	Mesh->MarkPendingKill();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestNotNull("Even pending kill", WeakMesh.Get(/*bEvenIfPendingKill =*/true));

	Component.MarkRenderStateDirty();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("The mesh goes with the proxy", WeakMesh.IsStale());
	TestTrue("The texture goes with the proxy", WeakTexture.IsStale());

	// No material: the default one.
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	(void)Component.SetStaticMesh(Cube);
	const FStaticMeshSceneProxy* CubeProxy = static_cast<const FStaticMeshSceneProxy*>(Component.SceneProxy);
	if (TestNotNull("A proxy for the cube", CubeProxy))
	{
		const FMaterial Default = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		TestTrue("The default material's values", CubeProxy->GetSectionMaterial(0).Albedo == Default.Albedo);
		TestTrue("The default material's map", CubeProxy->GetSectionMaterial(0).AlbedoMap == Default.AlbedoMap);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
