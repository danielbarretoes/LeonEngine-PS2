#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavigationWaypoint.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TriggerVolume.h"
#include "Engine/VisibilityCellVolume.h"
#include "Engine/VisibilityPortal.h"
#include "Engine/World.h"
#include "Factories/MapImportSettings.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "LeonEdTestUtils.h"
#include "Level/Light.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticLightingSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** The map importer's test scene (MakeMapFixture.py writes it). */
	FString MapFixture()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures/MapFixture.gltf")));
	}

	const TCHAR* const FixtureMap = TEXT("/LeonEdTest/Maps/MapFixture");

	/**
	 * A project's map rules for a scope, as its Config/DefaultEditor.ini would add them to the engine's: bomb sites and
	 * buy zones become tagged trigger volumes, and the maps need them (plan decision D15 keeps them out of the engine).
	 */
	class FScopedProjectMapRules
	{
	public:
		explicit FScopedProjectMapRules(const TArray<FString>& RequiredTags = DefaultRequiredTags())
			: Settings(*GetMutableDefault<UMapImportSettings>())
			, SavedRules(Settings.NodeRules)
			, SavedRequiredTags(Settings.RequiredTags)
		{
			for (const TCHAR* Zone : {TEXT("BombSite"), TEXT("BuyZone")})
			{
				FMapImportNodeRule Rule;
				Rule.Prefix = Zone;
				Rule.ActorClass = FSoftClassPath(ATriggerVolume::StaticClass());
				Rule.Tags.Add(FName(Zone));
				Rule.bSuffixAsTag = true;
				Settings.NodeRules.Add(Rule);
			}
			Settings.RequiredTags.Append(RequiredTags);
		}

		~FScopedProjectMapRules()
		{
			Settings.NodeRules = SavedRules;
			Settings.RequiredTags = SavedRequiredTags;
		}

		FScopedProjectMapRules(const FScopedProjectMapRules&) = delete;
		FScopedProjectMapRules& operator=(const FScopedProjectMapRules&) = delete;

		static TArray<FString> DefaultRequiredTags()
		{
			return {TEXT("BombSite+A"), TEXT("BombSite+B"), TEXT("BuyZone+CT"), TEXT("TriggerVolume:BuyZone+T"),
				TEXT("PlayerStart:CT"), TEXT("PlayerStart:T")};
		}

	private:
		UMapImportSettings& Settings;
		TArray<FMapImportNodeRule> SavedRules;
		TArray<FString> SavedRequiredTags;
	};

	/** The actor of the map's level named Name, as T. */
	template <typename T>
	T* FindActor(const UWorld& World, const TCHAR* Name)
	{
		return Cast<T>(StaticFindObjectFast(nullptr, World.PersistentLevel, FName(Name)));
	}

	/** Every file under the test content folder with its bytes, sorted by name. */
	TMap<FString, TArray<uint8>> ReadContentFiles()
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *LeonEdTest::GetContentDir(), TEXT("*.*"), true, false);
		Files.Sort();
		TMap<FString, TArray<uint8>> Contents;
		for (const FString& File : Files)
		{
			TArray<uint8> Bytes;
			(void)FFileHelper::LoadFileToArray(Bytes, *File);
			Contents.Add(File, MoveTemp(Bytes));
		}
		return Contents;
	}

	/** True when both sets of files have the same names and bytes. */
	bool SameFiles(const TMap<FString, TArray<uint8>>& A, const TMap<FString, TArray<uint8>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (const TPair<FString, TArray<uint8>>& Pair : A)
		{
			const TArray<uint8>* Other = B.Find(Pair.Key);
			if (Other == nullptr || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	UWorld* ImportFixtureMap()
	{
		return Cast<UWorld>(UImportAssetsCommandlet::ImportAsset(
			MapFixture(), FixtureMap, FString(), TEXT("Map"), TMap<FString, FString>()));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapImportSettingsTest, "System.LeonEd.MapFactory.SettingsFromConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapImportSettingsTest::RunTest(const FString& Parameters)
{
	// The engine's rules of BaseEditor.ini, and how a node name finds its rule and its suffix.
	const UMapImportSettings& Settings = *GetDefault<UMapImportSettings>();
	if (!TestEqual("The engine's rules", Settings.NodeRules.Num(), 7))
	{
		return false;
	}
	// N15's cells and portals.
	TestTrue("VIS_",
		Settings.NodeRules[5].Prefix == TEXT("VIS_") &&
			Settings.NodeRules[5].ActorClass.TryLoadClass<AActor>() == AVisibilityCellVolume::StaticClass());
	TestTrue("PORTAL_",
		Settings.NodeRules[6].Prefix == TEXT("PORTAL_") &&
			Settings.NodeRules[6].ActorClass.TryLoadClass<AActor>() == AVisibilityPortal::StaticClass());
	TestTrue("UCX_",
		Settings.NodeRules[0].Prefix == TEXT("UCX_") &&
			Settings.NodeRules[0].Kind == EMapImportNodeKind::ConvexCollision);
	TestTrue("COL_", Settings.NodeRules[1].Kind == EMapImportNodeKind::CollisionOnly);
	TestTrue("Clip_", Settings.NodeRules[2].ActorClass.TryLoadClass<AActor>() == ABlockingVolume::StaticClass());
	TestTrue("PlayerStart",
		Settings.NodeRules[3].ActorClass.TryLoadClass<AActor>() == APlayerStart::StaticClass() &&
			Settings.NodeRules[3].bSuffixAsTag);
	TestTrue(
		"NavWaypoint", Settings.NodeRules[4].ActorClass.TryLoadClass<AActor>() == ANavigationWaypoint::StaticClass());
	TestEqual("No engine required tags", Settings.RequiredTags.Num(), 0);

	TestTrue("A prefix, without case", Settings.FindRule(TEXT("clip_Roof")) == &Settings.NodeRules[2]);
	TestNull("No rule", Settings.FindRule(TEXT("Crate")));
	{
		FScopedProjectMapRules Rules;
		TestEqual("The project's rule", GetDefault<UMapImportSettings>()->FindRule(TEXT("BombSite_A"))->Prefix,
			FString(TEXT("BombSite")));
	}
	TestEqual("Suffix", UMapImportSettings::GetSuffix(TEXT("PlayerStart_CT"), TEXT("PlayerStart")), FString("CT"));
	TestEqual("Copy number", UMapImportSettings::GetSuffix(TEXT("BuyZone_T.001"), TEXT("BuyZone")), FString("T"));
	TestEqual("No suffix", UMapImportSettings::GetSuffix(TEXT("PlayerStart.002"), TEXT("PlayerStart")), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryConventionsTest, "System.LeonEd.MapFactory.ImportsEveryConvention",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryConventionsTest::RunTest(const FString& Parameters)
{
	// -type=Map makes the world of the map package -dest names: one actor per node, named after it, by the naming
	// rules; the meshes (shared ones once) and materials next to the map; glTF's metres and axes converted.
	LeonEdTest::FScopedTestContent Content;
	FScopedProjectMapRules Rules;
	UWorld* World = ImportFixtureMap();
	if (!TestNotNull("Imported", World))
	{
		return false;
	}
	TestEqual("The map package", World->GetOutermost()->GetName(), FString(FixtureMap));
	TestEqual("The world is named after it", World->GetName(), FString(TEXT("MapFixture")));
	const FString MapDir = LeonEdTest::GetContentDir() + TEXT("Maps/");
	TestTrue("A .lmap", FPaths::FileExists(MapDir + TEXT("MapFixture.lmap")));
	TestTrue("The shared mesh", FPaths::FileExists(MapDir + TEXT("MapFixture/Meshes/SM_Crate.lasset")));
	TestTrue("The floor mesh", FPaths::FileExists(MapDir + TEXT("MapFixture/Meshes/SM_Floor.lasset")));
	TestTrue("The material", FPaths::FileExists(MapDir + TEXT("MapFixture/Materials/M_Crate.lasset")));
	TestTrue("Its texture", FPaths::FileExists(MapDir + TEXT("MapFixture/Materials/T_MapFixture_Checker.lasset")));
	TestNotNull("The source", World->AssetImportData);
	TestTrue("Recorded",
		World->AssetImportData != nullptr &&
			World->AssetImportData->GetFirstFilename().EndsWith(TEXT("Fixtures/MapFixture.gltf")));

	// The world settings first, then the nodes in file order ("Group" and "Empty" have no mesh and no rule).
	const ULevel& Level = *World->PersistentLevel;
	if (!TestEqual("Actors", Level.Actors.Num(), 18))
	{
		return false;
	}
	TestTrue(
		"World settings first", Level.Actors[0] == Level.GetWorldSettings() && Level.GetWorldSettings() != nullptr);
	TestTrue("The first node next (Crate_B, written before its group)", Level.Actors[1]->GetName() == TEXT("Crate_B"));
	TestNull("A group is left out", FindActor<AActor>(*World, TEXT("Group")));
	TestNull("An empty is left out", FindActor<AActor>(*World, TEXT("Empty")));

	// Meshes: one SM_Crate for both crates; glTF (x, y, z) m is (x, z, y) * 100 cm, a child under its parent.
	const AStaticMeshActor* CrateA = FindActor<AStaticMeshActor>(*World, TEXT("Crate_A"));
	const AStaticMeshActor* CrateB = FindActor<AStaticMeshActor>(*World, TEXT("Crate_B"));
	if (TestNotNull("Crate_A", CrateA) && TestNotNull("Crate_B", CrateB))
	{
		UStaticMesh* Crate = CrateA->GetStaticMeshComponent()->GetStaticMesh();
		TestTrue("Shared mesh", Crate != nullptr && Crate == CrateB->GetStaticMeshComponent()->GetStaticMesh());
		TestEqual(
			"SM_Crate", Crate->GetPathName(), FString(TEXT("/LeonEdTest/Maps/MapFixture/Meshes/SM_Crate.SM_Crate")));
		TestTrue("Crate_A location", CrateA->GetActorLocation().Equals(FVector(200.0f, 0.0f, 50.0f), 1.0e-3f));
		TestTrue(
			"Crate_B under its group", CrateB->GetActorLocation().Equals(FVector(1000.0f, -200.0f, 50.0f), 1.0e-3f));
		TestTrue(
			"A 1 m cube is 100 cm", Crate->GetBoundingBox().GetSize().Equals(FVector(100.0f, 100.0f, 100.0f), 1.0e-3f));
		TestTrue("Static and colliding",
			CrateA->GetStaticMeshComponent()->Mobility == EComponentMobility::Static &&
				CrateA->GetStaticMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
		TestFalse("Visible", CrateA->IsHidden());
		// UCX_Crate_A_01: a 1.2 m box 10 cm above the crate's centre, in the mesh's simple collision.
		const UBodySetup* Body = Crate->GetBodySetup();
		if (TestTrue("One convex box", Body != nullptr && Body->AggGeom.BoxElems.Num() == 1))
		{
			const FKBoxElem& Box = Body->AggGeom.BoxElems[0];
			TestTrue("Box centre", Box.Center.Equals(FVector(0.0f, 0.0f, 10.0f), 1.0e-3f));
			TestTrue("Box size", FVector(Box.X, Box.Y, Box.Z).Equals(FVector(120.0f, 120.0f, 120.0f), 1.0e-3f));
			TestTrue("The box answers traces", Body->CollisionTraceFlag == CTF_UseSimpleAsComplex);
		}
		// The textured material, in the map's Materials folder.
		const UMaterial* Material = Cast<UMaterial>(Crate->GetMaterial(0));
		if (TestNotNull("M_Crate", Material))
		{
			TestEqual("M_Crate path", Material->GetPathName(),
				FString(TEXT("/LeonEdTest/Maps/MapFixture/Materials/M_Crate.M_Crate")));
			TestTrue("Base colour map",
				Material->BaseColorMap != nullptr && Material->BaseColorMap->GetName() == TEXT("T_MapFixture_Checker"));
		}
	}
	TestNull("A UCX_ node is no actor", FindActor<AActor>(*World, TEXT("UCX_Crate_A_01")));
	const AStaticMeshActor* Wall = FindActor<AStaticMeshActor>(*World, TEXT("COL_Wall"));
	TestTrue("COL_: hidden, colliding",
		Wall != nullptr && Wall->IsHidden() && Wall->GetStaticMeshComponent()->IsCollisionEnabled());
	const AStaticMeshActor* Floor = FindActor<AStaticMeshActor>(*World, TEXT("Floor"));
	TestTrue("No rule: a static mesh",
		Floor != nullptr && Floor->GetStaticMeshComponent()->GetStaticMesh() != nullptr &&
			Floor->GetStaticMeshComponent()->GetStaticMesh()->GetName() == TEXT("SM_Floor"));

	// Volumes: the box of the node's mesh, sized by the node's scale.
	const ABlockingVolume* Clip = FindActor<ABlockingVolume>(*World, TEXT("Clip_Edge"));
	if (TestNotNull("Clip_: a blocking volume", Clip))
	{
		TestTrue("Clip location", Clip->GetActorLocation().Equals(FVector(0.0f, 500.0f, 100.0f), 1.0e-3f));
		TestTrue("Clip box", Clip->GetBrushBounds().GetExtent().Equals(FVector(200.0f, 50.0f, 200.0f), 1.0e-2f));
		TestTrue("Clip collides", Clip->GetBrushComponent()->IsCollisionEnabled());
	}
	const ATriggerVolume* SiteA = FindActor<ATriggerVolume>(*World, TEXT("BombSite_A"));
	if (TestNotNull("BombSite_A: a trigger volume", SiteA))
	{
		TestTrue("Site tags",
			SiteA->Tags.Num() == 2 && SiteA->Tags[0] == FName(TEXT("BombSite")) && SiteA->Tags[1] == FName(TEXT("A")));
		TestTrue("Site location", SiteA->GetActorLocation().Equals(FVector(800.0f, -800.0f, 100.0f), 1.0e-3f));
		TestTrue("Site box", SiteA->GetBrushBounds().GetExtent().Equals(FVector(200.0f, 200.0f, 100.0f), 1.0e-2f));
	}
	const ATriggerVolume* BuyT = FindActor<ATriggerVolume>(*World, TEXT("BuyZone_T_001"));
	TestTrue("A copy number is dropped from the tag",
		BuyT != nullptr && BuyT->ActorHasTag(FName(TEXT("BuyZone"))) && BuyT->ActorHasTag(FName(TEXT("T"))));

	// Player starts: the suffix is the PlayerStartTag; upright, facing the node's +X.
	const APlayerStart* StartCT = FindActor<APlayerStart>(*World, TEXT("PlayerStart_CT"));
	const APlayerStart* StartT = FindActor<APlayerStart>(*World, TEXT("PlayerStart_T"));
	if (TestNotNull("PlayerStart_CT", StartCT) && TestNotNull("PlayerStart_T", StartT))
	{
		TestEqual("CT tag", StartCT->PlayerStartTag, FName(TEXT("CT")));
		TestEqual("T tag", StartT->PlayerStartTag, FName(TEXT("T")));
		TestTrue("CT location", StartCT->GetActorLocation().Equals(FVector(-800.0f, -800.0f, 92.0f), 1.0e-3f));
		TestEqual("CT faces +Y", StartCT->GetActorRotation().Yaw, 90.0f, 1.0e-2f);
		TestEqual("T faces -X", FMath::Abs(StartT->GetActorRotation().Yaw), 180.0f, 1.0e-2f);
		TestEqual("Upright", StartCT->GetActorRotation().Pitch, 0.0f, 0.0f);
	}

	// Waypoints: the extras' links (a string, an array) and flags.
	const ANavigationWaypoint* W1 = FindActor<ANavigationWaypoint>(*World, TEXT("NavWaypoint_01"));
	const ANavigationWaypoint* W2 = FindActor<ANavigationWaypoint>(*World, TEXT("NavWaypoint_02"));
	const ANavigationWaypoint* W3 = FindActor<ANavigationWaypoint>(*World, TEXT("NavWaypoint_03"));
	if (TestNotNull("NavWaypoint_01", W1) && TestNotNull("NavWaypoint_02", W2) && TestNotNull("NavWaypoint_03", W3))
	{
		TestTrue("01 links 02 and 03", W1->Links.Num() == 2 && W1->Links[0] == W2 && W1->Links[1] == W3);
		TestTrue("01 flags", W1->Flags.Num() == 1 && W1->HasFlag(FName(TEXT("Jump"))));
		TestTrue("02 links 01", W2->Links.Num() == 1 && W2->Links[0] == W1);
		TestTrue("02 flags", W2->Flags.Num() == 2 && W2->HasFlag(FName(TEXT("Crouch"))));
		TestTrue("03 has none", W3->Links.Num() == 0 && W3->Flags.Num() == 0);
		TestTrue("02 location", W2->GetActorLocation().Equals(FVector(400.0f, 0.0f, 0.0f), 1.0e-3f));
		TestTrue("03 location", W3->GetActorLocation().Equals(FVector(0.0f, 400.0f, 0.0f), 1.0e-3f));
	}

	// Lights: KHR_lights_punctual, whatever their names; a spot light is a point light.
	const ADirectionalLight* Sun = FindActor<ADirectionalLight>(*World, TEXT("Sun"));
	if (TestNotNull("Sun", Sun))
	{
		const FVector Expected = FVector(0.3f, 0.2f, -1.0f).GetSafeNormal();
		TestTrue(
			"Sun direction (glTF -Z of the node)", Sun->GetLightComponent()->GetDirection().Equals(Expected, 1.0e-4f));
		TestEqual("Sun intensity", Sun->GetLightComponent()->Intensity, 1.5f);
		TestTrue("Sun shadows", Sun->GetLightComponent()->CastShadows);
		TestTrue("Sun colour", Sun->GetLightComponent()->LightColor == FLinearColor(1.0f, 0.95f, 0.9f));
	}
	const APointLight* Lamp = FindActor<APointLight>(*World, TEXT("Lamp"));
	if (TestNotNull("Lamp", Lamp))
	{
		TestTrue("Lamp location", Lamp->GetActorLocation().Equals(FVector(100.0f, 100.0f, 300.0f), 1.0e-3f));
		TestEqual("Lamp range", Lamp->GetPointLightComponent()->AttenuationRadius, 500.0f, 1.0e-3f);
		TestEqual("Lamp intensity", Lamp->GetPointLightComponent()->Intensity, 2.0f);
		// Every light casts the baked lighting's shadows (N22).
		TestTrue("Lamp shadows", Lamp->GetPointLightComponent()->CastShadows);
	}
	const APointLight* Spot = FindActor<APointLight>(*World, TEXT("Spot"));
	TestTrue("Spot: a point light with the default range",
		Spot != nullptr && Spot->GetPointLightComponent()->AttenuationRadius == DefaultPointLightRange);

	// The saved map loads back as a map package.
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	UPackage* Loaded = LoadPackage(nullptr, FixtureMap, LOAD_None);
	TestTrue("PKG_ContainsMap", Loaded != nullptr && Loaded->ContainsMap());
	const UWorld* LoadedWorld = UWorld::FindWorldInPackage(Loaded);
	TestTrue("The loaded map", LoadedWorld != nullptr && LoadedWorld->PersistentLevel->Actors.Num() == 18);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryReimportTest, "System.LeonEd.MapFactory.ReimportIsReproducible",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryReimportTest::RunTest(const FString& Parameters)
{
	// Gate G5 for maps: importing over the map in memory, and reimporting it after a load (-reimport), rebuild its
	// level in place and save the same bytes, meshes and materials included.
	LeonEdTest::FScopedTestContent Content;
	FScopedProjectMapRules Rules;
	if (!TestNotNull("Imported", ImportFixtureMap()))
	{
		return false;
	}
	const TMap<FString, TArray<uint8>> First = ReadContentFiles();
	TestEqual("Files (the map, 3 meshes, 2 materials, a texture)", First.Num(), 7);
	TestNotNull("Imported over itself", ImportFixtureMap());
	TestTrue("The same bytes", SameFiles(First, ReadContentFiles()));

	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	TArray<FString> Packages;
	Packages.Add(FixtureMap);
	int32 Reimported = 0;
	TestEqual("Reimport", UImportAssetsCommandlet::ReimportPackages(Packages, &Reimported), 0);
	TestEqual("The map reimported", Reimported, 1);
	TestTrue("The same bytes after a load", SameFiles(First, ReadContentFiles()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryBakesStaticLightingTest,
	"System.LeonEd.MapFactory.BakesStaticLighting",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryBakesStaticLightingTest::RunTest(const FString& Parameters)
{
	// The import bakes the map's static lighting (N22): every static mesh it places has colours made for its mesh,
	// saved with the map and loaded back the same.
	LeonEdTest::FScopedTestContent Content;
	FScopedProjectMapRules Rules;
	UWorld* World = ImportFixtureMap();
	if (!TestNotNull("Imported", World))
	{
		return false;
	}
	TMap<FString, TArray<uint8>> Imported;
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		const UStaticMeshComponent* Component = MeshActor != nullptr ? MeshActor->GetStaticMeshComponent() : nullptr;
		if (Component == nullptr || !FStaticLightingSystem::ReceivesStaticLighting(*Component))
		{
			continue;
		}
		TestTrue(*FString::Printf("%s is baked", *Actor->GetName()), Component->HasValidBakedVertexColors());
		Imported.Add(Actor->GetName(), Component->BakedVertexColors.Data);
	}
	TestTrue("Static meshes baked", Imported.Num() >= 3);

	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const UWorld* Loaded = UWorld::FindWorldInPackage(LoadPackage(nullptr, FixtureMap, LOAD_None));
	if (!TestNotNull("Loaded", Loaded))
	{
		return false;
	}
	int32 NumSame = 0;
	for (const AActor* Actor : Loaded->PersistentLevel->Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		const TArray<uint8>* Colors = MeshActor != nullptr ? Imported.Find(Actor->GetName()) : nullptr;
		if (Colors == nullptr)
		{
			continue;
		}
		const UStaticMeshComponent& Component = *MeshActor->GetStaticMeshComponent();
		NumSame += Component.HasValidBakedVertexColors() && Component.BakedVertexColors.Data == *Colors ? 1 : 0;
	}
	TestEqual("Every baked mesh loads its colours", NumSame, Imported.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryAutoLinksWaypointsTest, "System.LeonEd.MapFactory.AutoLinksWaypoints",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryAutoLinksWaypointsTest::RunTest(const FString& Parameters)
{
	// bAutoLinkWaypoints (plan P20): the import adds the links an agent can walk to the ones the nodes name,
	// NavWaypoint_03 (which names none) gets some, every added link but a drop goes both ways, A* finds a way over
	// the imported graph, and the import stays reproducible.
	LeonEdTest::FScopedTestContent Content;
	FScopedProjectMapRules Rules;
	UMapImportSettings& Settings = *GetMutableDefault<UMapImportSettings>();
	const bool bSaved = Settings.bAutoLinkWaypoints;
	Settings.bAutoLinkWaypoints = false;
	UWorld* Plain = ImportFixtureMap();
	int32 PlainLinks = 0;
	for (const TCHAR* Name : {TEXT("NavWaypoint_01"), TEXT("NavWaypoint_02"), TEXT("NavWaypoint_03")})
	{
		const ANavigationWaypoint* Waypoint = Plain != nullptr ? FindActor<ANavigationWaypoint>(*Plain, Name) : nullptr;
		PlainLinks += Waypoint != nullptr ? Waypoint->Links.Num() : 0;
	}
	Settings.bAutoLinkWaypoints = true;
	UWorld* World = ImportFixtureMap();
	const TMap<FString, TArray<uint8>> First = ReadContentFiles();
	const bool bReproducible = ImportFixtureMap() != nullptr && SameFiles(First, ReadContentFiles());
	Settings.bAutoLinkWaypoints = bSaved;
	if (!TestNotNull("Imported", World))
	{
		return false;
	}
	const ANavigationWaypoint* W1 = FindActor<ANavigationWaypoint>(*World, TEXT("NavWaypoint_01"));
	const ANavigationWaypoint* W3 = FindActor<ANavigationWaypoint>(*World, TEXT("NavWaypoint_03"));
	if (!TestNotNull("NavWaypoint_01", W1) || !TestNotNull("NavWaypoint_03", W3))
	{
		return false;
	}
	int32 Links = 0;
	for (const TCHAR* Name : {TEXT("NavWaypoint_01"), TEXT("NavWaypoint_02"), TEXT("NavWaypoint_03")})
	{
		const ANavigationWaypoint* Waypoint = FindActor<ANavigationWaypoint>(*World, Name);
		Links += Waypoint != nullptr ? Waypoint->Links.Num() : 0;
	}
	TestTrue("Links were added", Links > PlainLinks);
	TestTrue("NavWaypoint_03 is linked now", W3->Links.Num() > 0);
	TestTrue("Both ways", W3->Links.Num() == 0 || W3->Links[0]->Links.Contains(W3));
	TestTrue("Reproducible", bReproducible);

	UNavigationSystem Navigation;
	Navigation.Build(*World);
	TArray<int32> NodePath;
	TestTrue("A* from NavWaypoint_03 to NavWaypoint_01",
		UNavigationSystem::FindNodePath(
			Navigation.GetNodes(), Navigation.FindNode(W3), Navigation.FindNode(W1), NodePath) &&
			NodePath.Num() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryRequiredTagsTest,
	"System.LeonEd.MapFactory.RequiredTagsFailTheImport",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryRequiredTagsTest::RunTest(const FString& Parameters)
{
	// A project's RequiredTags that no actor meets fail the import, naming each missing entry; nothing is saved.
	LeonEdTest::FScopedTestContent Content;
	TArray<FString> Required = FScopedProjectMapRules::DefaultRequiredTags();
	Required.Add(TEXT("BombSite+C"));
	Required.Add(TEXT("PlayerStart:BombSite"));
	FScopedProjectMapRules Rules(Required);
	AddExpectedError(TEXT("has nothing with the required tags 'BombSite+C'"), 1);
	AddExpectedError(TEXT("has nothing with the required tags 'PlayerStart:BombSite'"), 1);
	AddExpectedError(TEXT("failed to import"), 1);
	TestNull("The import fails", ImportFixtureMap());
	TestFalse("Nothing saved", FPaths::FileExists(LeonEdTest::GetContentDir() + TEXT("Maps/MapFixture.lmap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryEngineMapsSkipRequiredTagsTest,
	"System.LeonEd.MapFactory.EngineMapsSkipRequiredTags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryEngineMapsSkipRequiredTagsTest::RunTest(const FString& Parameters)
{
	// RequiredTags are a project's check of its maps: a reimport of everything with a project (gate G5) does not check
	// the engine's maps (AxisTest has no bomb site) against the project's rules, nor the project's maps listed in
	// MapsWithoutRequiredTags (ShooterGame's MainMenu).
	TestTrue("A project's map", UMapImportSettings::AppliesRequiredTags(TEXT("/Game/Maps/de_leon")));
	TestTrue("A test mount's map", UMapImportSettings::AppliesRequiredTags(TEXT("/LeonEdTest/Maps/MapFixture")));
	TestFalse("An engine map", UMapImportSettings::AppliesRequiredTags(TEXT("/Engine/Maps/AxisTest")));
	UMapImportSettings* Settings = GetMutableDefault<UMapImportSettings>();
	const TArray<FString> SavedExempt = Settings->MapsWithoutRequiredTags;
	Settings->MapsWithoutRequiredTags = {TEXT("/Game/Maps/MainMenu")};
	TestFalse("An exempt map", UMapImportSettings::AppliesRequiredTags(TEXT("/Game/Maps/mainmenu")));
	TestTrue("Only that one", UMapImportSettings::AppliesRequiredTags(TEXT("/Game/Maps/de_leon")));
	Settings->MapsWithoutRequiredTags = SavedExempt;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryCellsAndPortalsTest, "System.LeonEd.MapFactory.CellsAndPortals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryCellsAndPortalsTest::RunTest(const FString& Parameters)
{
	// N15: VIS_<Cell> nodes become cell volumes named after their suffix (the box of their mesh), PORTAL_<A>_<B> nodes
	// portals between two of them (the rectangle of their quad, its corners in the world; a cell name may hold an
	// underscore), and a portal to a cell the map lacks is left out; saved with the map and loaded back.
	LeonEdTest::FScopedTestContent Content;
	const FString Fixture = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures/CellsFixture.gltf")));
	const TCHAR* const MapName = TEXT("/LeonEdTest/Maps/CellsFixture");
	UWorld* World = Cast<UWorld>(
		UImportAssetsCommandlet::ImportAsset(Fixture, MapName, FString(), TEXT("Map"), TMap<FString, FString>()));
	if (!TestNotNull("Imported", World))
	{
		return false;
	}
	const AVisibilityCellVolume* Hall = FindActor<AVisibilityCellVolume>(*World, TEXT("VIS_Hall"));
	const AVisibilityCellVolume* RoomB = FindActor<AVisibilityCellVolume>(*World, TEXT("VIS_Room_B"));
	if (!TestNotNull("VIS_Hall: a cell", Hall) || !TestNotNull("VIS_Room_B: a cell", RoomB))
	{
		return false;
	}
	TestEqual("The hall's name", Hall->CellName, FName(TEXT("Hall")));
	TestEqual("A name with an underscore", RoomB->CellName, FName(TEXT("Room_B")));
	const FBox HallBox = Hall->GetBrushBounds();
	TestTrue("The hall's box",
		HallBox.Min.Equals(FVector(0.0f, 0.0f, 0.0f), 0.1f) &&
			HallBox.Max.Equals(FVector(1000.0f, 1000.0f, 300.0f), 0.1f));
	const AVisibilityPortal* Door = FindActor<AVisibilityPortal>(*World, TEXT("PORTAL_Hall_Room_B"));
	if (TestNotNull("PORTAL_Hall_Room_B: a portal", Door))
	{
		TestEqual("From the hall", Door->CellA, FName(TEXT("Hall")));
		TestEqual("To room B", Door->CellB, FName(TEXT("Room_B")));
		FBox DoorBox(ForceInit);
		for (const FVector& Corner : Door->Corners)
		{
			DoorBox += Corner;
		}
		TestEqual("Four corners", Door->Corners.Num(), 4);
		TestTrue("The door in the wall",
			DoorBox.Min.Equals(FVector(1000.0f, 450.0f, 0.0f), 0.1f) &&
				DoorBox.Max.Equals(FVector(1000.0f, 550.0f, 200.0f), 0.1f));
		TestTrue("Corners around it (the second and third share a side)",
			FVector::Dist(Door->Corners[0], Door->Corners[2]) > 200.0f);
	}
	TestNull("A portal to no cell is left out", FindActor<AActor>(*World, TEXT("PORTAL_Hall_Nowhere")));

	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const UWorld* Loaded = UWorld::FindWorldInPackage(LoadPackage(nullptr, MapName, LOAD_None));
	const AVisibilityPortal* LoadedDoor =
		Loaded != nullptr ? FindActor<AVisibilityPortal>(*Loaded, TEXT("PORTAL_Hall_Room_B")) : nullptr;
	TestTrue("Saved and loaded",
		LoadedDoor != nullptr && LoadedDoor->Corners.Num() == 4 && LoadedDoor->CellB == FName(TEXT("Room_B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMapFactoryWorldSettingsTest, "System.LeonEd.MapFactory.WorldSettingsFromNode",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMapFactoryWorldSettingsTest::RunTest(const FString& Parameters)
{
	// The WorldSettings node's extras set the map's AWorldSettings by property path (ps2-polish P8): the fog, the sky's
	// cube map by its object path, a number, a bool; a key that names no property fails the import.
	LeonEdTest::FScopedTestContent Content;
	UPackage* SkyPackage = CreatePackage(TEXT("/LeonEdTest/Sky/T_Sky"));
	UTextureCube* Sky = NewObject<UTextureCube>(SkyPackage, FName(TEXT("T_Sky")), RF_Public | RF_Standalone);
	const auto Gltf = [](const FString& Extras)
	{
		return FString::Printf("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
							   "\"nodes\":[{\"name\":\"WorldSettings\",\"extras\":%s}]}",
			*Extras);
	};
	const FString Good = LeonEdTest::WriteSource(TEXT("Maps/Settings.gltf"),
		Gltf(TEXT("{\"FogSettings.bEnableFog\":true,\"FogSettings.StartDistance\":3000,"
				  "\"FogSettings.EndDistance\":\"9000\",\"SkySettings.SkyCubemap\":\"/LeonEdTest/Sky/T_Sky.T_Sky\","
				  "\"KillZ\":-5000}")));
	UWorld* World = Cast<UWorld>(UImportAssetsCommandlet::ImportAsset(
		Good, TEXT("/LeonEdTest/Maps/Settings"), FString(), TEXT("Map"), TMap<FString, FString>()));
	const AWorldSettings* Settings = World != nullptr ? World->GetWorldSettings() : nullptr;
	if (!TestNotNull("Imported, with world settings", Settings))
	{
		return false;
	}
	TestTrue("The fog on", Settings->FogSettings.bEnableFog);
	TestEqual("The fog's start", Settings->FogSettings.StartDistance, 3000.0f);
	TestEqual("The fog's end (a string)", Settings->FogSettings.EndDistance, 9000.0f);
	TestTrue("The sky", Settings->SkySettings.SkyCubemap == Sky);
	TestEqual("KillZ", Settings->KillZ, -5000.0f);
	TestNull("No actor for the node", FindActor<AActor>(*World, TEXT("WorldSettings_2")));

	const FString Bad = LeonEdTest::WriteSource(TEXT("Maps/Bad.gltf"), Gltf(TEXT("{\"FogSettings.NoSuchThing\":1}")));
	AddExpectedError(TEXT("is no world settings property"), 1);
	AddExpectedError(TEXT("failed to import"), 1);
	TestNull("A key that names no property",
		UImportAssetsCommandlet::ImportAsset(
			Bad, TEXT("/LeonEdTest/Maps/Bad"), FString(), TEXT("Map"), TMap<FString, FString>()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
