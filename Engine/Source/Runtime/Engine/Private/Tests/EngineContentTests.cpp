#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Level/BasicShape.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

// The engine content (Engine/Content, `/Engine` packages since P14 part 2): the defaults the config names, the basic
// shapes, the migrated materials and the source of the imported texture; and the garbage-collection safety of the
// assets the scene proxies draw.

namespace
{

	/** The engine config the defaults come from: GEngine's, or the class default object's. */
	const UEngine& GetEngineConfig()
	{
		return GEngine != nullptr ? *GEngine : *GetDefault<UEngine>();
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineContentDefaultsTest, "System.Engine.EngineContent.DefaultsFromConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineContentDefaultsTest::RunTest(const FString& Parameters)
{
	// BaseEngine.ini names the default assets on UEngine; they load from their packages, where LoadObject finds them
	// like any other asset. The default material is loaded once and rooted.
	const UEngine& Engine = GetEngineConfig();
	TestEqual("DefaultMaterialName from the config", Engine.DefaultMaterialName.ToString(),
		FString("/Engine/EngineMaterials/M_Default.M_Default"));
	TestEqual("DefaultTextureName from the config", Engine.DefaultTextureName.ToString(),
		FString("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	TestEqual("DefaultBumpNormalTextureName from the config", Engine.DefaultBumpNormalTextureName.ToString(),
		FString("/Engine/EngineMaterials/T_Default_Bump_N.T_Default_Bump_N"));
	TestTrue("No UI sounds in the engine config",
		Engine.UIClickSoundName.IsNull() && Engine.UIConfirmSoundName.IsNull() && Engine.UIBackSoundName.IsNull() &&
			Engine.UIErrorSoundName.IsNull());

	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull("Default material", DefaultMaterial))
	{
		return false;
	}
	TestEqual("At its config path", DefaultMaterial->GetPathName(), Engine.DefaultMaterialName.ToString());
	TestTrue("Rooted", DefaultMaterial->IsRooted());
	TestTrue("Loaded from its package",
		DefaultMaterial->HasAnyFlags(RF_WasLoaded) &&
			FPackageName::DoesPackageExist(TEXT("/Engine/EngineMaterials/M_Default")));
	TestTrue("Made once", UMaterial::GetDefaultMaterial(MD_Surface) == DefaultMaterial);
	TestTrue("LoadObject finds it",
		LoadObject<UMaterial>(nullptr, *Engine.DefaultMaterialName.ToString()) == DefaultMaterial);
	// M_Default.lmat: white, 8 shininess, the engine's T_Default_D.
	TestTrue("M_Default's base colour", DefaultMaterial->BaseColor.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	TestEqual("M_Default's shininess", DefaultMaterial->Shininess, 8.0f);
	TestTrue("M_Default's map",
		DefaultMaterial->BaseColorMap != nullptr &&
			DefaultMaterial->BaseColorMap->GetPathName() == TEXT("/Engine/EngineMaterials/T_Default_D.T_Default_D"));

	const UTexture2D* Checker = LoadObject<UTexture2D>(nullptr, *Engine.DefaultTextureName.ToString());
	if (TestNotNull("DefaultTexture", Checker))
	{
		TestEqual("Checker size", Checker->GetSizeX(), 64);
		TestEqual("Checker is sRGB", static_cast<int32>(Checker->SRGB), 1);
	}
	const UTexture2D* Bump = LoadObject<UTexture2D>(nullptr, *Engine.DefaultBumpNormalTextureName.ToString());
	if (TestNotNull("Bump map", Bump))
	{
		TestEqual("Bump size", Bump->GetSizeX(), 256);
		TestEqual("A normal map is not sRGB", static_cast<int32>(Bump->SRGB), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineContentBasicShapesTest, "System.Engine.EngineContent.BasicShapes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineContentBasicShapesTest::RunTest(const FString& Parameters)
{
	// The basic shapes load from /Engine/BasicShapes with the procedural generator's geometry and no material slots;
	// a sphere of another tessellation is built at run time, once per tessellation.
	const UStaticMesh* Cube = MeshForBasicShape(EBasicShape::Cube);
	const UStaticMesh* Plane = MeshForBasicShape(EBasicShape::Plane);
	if (TestNotNull("Cube", Cube) && TestNotNull("Plane", Plane))
	{
		TestEqual("Cube path", Cube->GetPathName(), FString("/Engine/BasicShapes/Cube.Cube"));
		TestEqual("Cube triangles", Cube->GetNumTriangles(), 12);
		TestEqual("Cube size (cm)", Cube->GetBoundingBox().Max.X - Cube->GetBoundingBox().Min.X, 100.0f, 1.0e-3f);
		TestEqual("The shapes have no slots", Cube->GetStaticMaterials().Num(), 0);
		TestEqual("Plane size (cm)", Plane->GetBoundingBox().Max.Y - Plane->GetBoundingBox().Min.Y, 100.0f, 1.0e-3f);
		const FMeshData Generated = MakeCube();
		TestTrue("The generator's vertices",
			Cube->GetLODResources().Vertices.Num() == Generated.Vertices.Num() &&
				Cube->GetLODResources().Vertices[5].Position == Generated.Vertices[5].Position &&
				Cube->GetLODResources().Vertices[5].Tangent == Generated.Vertices[5].Tangent);
	}
	UStaticMesh* Sphere = GetSphereMesh(24, 16);
	UStaticMesh* CoarseSphere = GetSphereMesh(8, 6);
	if (TestNotNull("Sphere", Sphere) && TestNotNull("Coarse sphere", CoarseSphere))
	{
		TestEqual("The default sphere", Sphere->GetPathName(), FString("/Engine/BasicShapes/Sphere.Sphere"));
		TestTrue(
			"Another tessellation is another mesh", CoarseSphere != Sphere && CoarseSphere->HasAnyFlags(RF_Transient));
		TestTrue("Coarser", CoarseSphere->GetNumTriangles() < Sphere->GetNumTriangles());
		TestTrue("Cached per tessellation", GetSphereMesh(8, 6) == CoarseSphere);
		TestEqual("The generator's triangles", Sphere->GetNumTriangles(), MakeSphere(24, 16).Indices.Num() / 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineContentMigratedTest, "System.Engine.EngineContent.MigratedContent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineContentMigratedTest::RunTest(const FString& Parameters)
{
	// The .lmat materials became M_ packages with the same parameters; the template map's plane shows the one its
	// level named; T_Default_D keeps its source in Engine/SourceArt, relative to the engine and with its MD5 (a
	// reimport writes the same bytes, G5).
	const UMaterial* WorldGrid =
		LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/M_WorldGrid.M_WorldGrid"));
	if (TestNotNull("M_WorldGrid", WorldGrid))
	{
		TestTrue("UVScale 8", WorldGrid->UVScale == FVector2D(8.0f, 8.0f));
		TestEqual("Roughness as written", WorldGrid->Roughness, 0.4472135954999579f, 0.0f);
		TestTrue("Its map",
			WorldGrid->BaseColorMap != nullptr &&
				WorldGrid->BaseColorMap->GetPathName() == TEXT("/Engine/EngineMaterials/T_Default_D.T_Default_D"));
	}
	const UMaterial* SolidMetal =
		LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/M_SolidMetal.M_SolidMetal"));
	if (TestNotNull("M_SolidMetal", SolidMetal))
	{
		TestEqual("Metallic", SolidMetal->Metallic, 1.0f);
		TestEqual("Roughness", SolidMetal->Roughness, 0.35f);
		TestNull("No map", SolidMetal->BaseColorMap);
	}
	UPackage* Starter = LoadPackage(nullptr, TEXT("/Engine/Maps/Template_Default"), LOAD_None);
	const UWorld* StarterWorld = UWorld::FindWorldInPackage(Starter);
	const AStaticMeshActor* Plane = StarterWorld != nullptr ? StarterWorld->FindFirst<AStaticMeshActor>() : nullptr;
	TestTrue("The template's plane shows M_WorldGrid",
		Plane != nullptr && Plane->GetStaticMeshComponent()->GetMaterial(0) == WorldGrid);

	const UTexture2D* Texture =
		LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineMaterials/T_Default_D.T_Default_D"));
	if (TestNotNull("T_Default_D", Texture))
	{
		TestEqual("128 x 128", Texture->GetSizeX(), 128);
	#if WITH_EDITORONLY_DATA
		const UAssetImportData* ImportData = Texture->AssetImportData;
		if (TestNotNull("Import data", ImportData))
		{
			TestEqual("Relative to the engine", ImportData->SourceData.SourceFiles[0].RelativeFilename,
				FString("SourceArt/EngineMaterials/T_Default_D.png"));
			const FString Source = ImportData->GetFirstFilename();
			TestTrue("The source exists", FPaths::FileExists(Source));
			TestEqual("Its MD5", ImportData->GetFirstFileHash(), UAssetImportData::HashFile(Source));
		}
	#endif
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenePrimitiveAssetsSurviveTest, "System.Engine.EngineContent.ProxiesKeepTheirAssets",
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
