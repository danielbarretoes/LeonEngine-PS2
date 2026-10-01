#include "AI/Navigation/NavigationWaypoint.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameMapsSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysScene.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerState.h"
#include "UI/ShooterMainMenuWidget.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// The maps (de_leon, de_puerto) and their rules: each imported map holds what the game needs and its waypoint graph
// covers it, the project's RequiredTags reject a map without it, and a headless match on each places ten pawns at their
// teams' starts (gate G6 in a test).

namespace
{

	const TCHAR* const DeLeon = TEXT("/Game/Maps/de_leon");
	const TCHAR* const DePuerto = TEXT("/Game/Maps/de_puerto");

	/** A mount point over a fresh folder of the project's Intermediate directory, for imports (never Content). */
	class FScopedShooterTestContent
	{
	public:
		static constexpr const TCHAR* Root = TEXT("/ShooterGameTest/");

		FScopedShooterTestContent()
			: ContentDir(FPaths::ProjectIntermediateDir() + TEXT("Tests/ShooterGame/Content/"))
		{
			IFileManager::Get().DeleteDirectory(*ContentDir, false, true);
			IFileManager::Get().MakeDirectory(*ContentDir, true);
			FPackageName::RegisterMountPoint(Root, ContentDir);
		}

		~FScopedShooterTestContent()
		{
			// Every object of the test packages goes, as a new process would start without them.
			TArray<UPackage*> Packages;
			for (TObjectIterator<UPackage> It; It; ++It)
			{
				if (It->GetName().StartsWith(Root))
				{
					Packages.Add(*It);
				}
			}
			for (UPackage* Package : Packages)
			{
				TArray<UObject*> Objects;
				GetObjectsWithOuter(Package, Objects, true);
				for (UObject* Object : Objects)
				{
					Object->RemoveFromRoot();
					Object->MarkPendingKill();
				}
				Package->MarkPendingKill();
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
			FPackageName::UnRegisterMountPoint(Root, ContentDir);
			IFileManager::Get().DeleteDirectory(*ContentDir, false, true);
		}

		FScopedShooterTestContent(const FScopedShooterTestContent&) = delete;
		FScopedShooterTestContent& operator=(const FScopedShooterTestContent&) = delete;

	private:
		FString ContentDir;
	};

	/** A source file of the repository, from the project folder. */
	FString SourceArtFile(const TCHAR* RelativeToProject)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RelativeToProject));
	}

	/** The actors of a class in a world's level, with a tag (NAME_None: any). */
	template <typename T>
	TArray<T*> FindActors(const UWorld& World, FName Tag = NAME_None)
	{
		TArray<T*> Found;
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			if (T* Typed = Cast<T>(Actor))
			{
				if (Tag == NAME_None || Actor->ActorHasTag(Tag))
				{
					Found.Add(Typed);
				}
			}
		}
		return Found;
	}

	/** The player starts of a team tag. */
	TArray<APlayerStart*> FindTeamStarts(const UWorld& World, const TCHAR* TeamTag)
	{
		TArray<APlayerStart*> Starts;
		for (APlayerStart* Start : FindActors<APlayerStart>(World))
		{
			if (Start->PlayerStartTag == FName(TeamTag))
			{
				Starts.Add(Start);
			}
		}
		return Starts;
	}

	/** What a map of the game holds besides the required tags (ShooterGame.Map.*HoldsTheGame). */
	struct FExpectedMap
	{
		const TCHAR* Name;
		int32 NumLookouts;
		int32 NumClimbs;
		int32 NumClips;
		int32 NumLadders;
		/** The CT starts stand at least this far from Y = 0, out of mid's line (cm). */
		float MinCTStartY;
	};

	/**
	 * The imported map is the main menu's (its map list) and holds two bomb sites, the teams' buy zones and five starts
	 * a team, the waypoint graph with its links, the lookouts and the ladders' climbs, its player clips, ladders and
	 * sun.
	 */
	bool TestMapHoldsTheGame(FAutomationTestBase& Test, const FExpectedMap& Expected)
	{
		Test.TestTrue(
			"The main menu's map", GetDefault<UShooterMainMenuWidget>()->MapNames.Contains(FString(Expected.Name)));
		UPackage* Package = LoadPackage(nullptr, Expected.Name, LOAD_None);
		UWorld* World = Package != nullptr ? UWorld::FindWorldInPackage(Package) : nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s loads"), Expected.Name), World))
		{
			return false;
		}
		for (const TCHAR* Site : {TEXT("A"), TEXT("B")})
		{
			bool bFound = false;
			for (const ATriggerVolume* Volume : FindActors<ATriggerVolume>(*World, FName(TEXT("BombSite"))))
			{
				bFound |= Volume->ActorHasTag(FName(Site));
			}
			Test.TestTrue(*FString::Printf(TEXT("Bomb site %s"), Site), bFound);
		}
		Test.TestEqual("Two bomb sites", FindActors<ATriggerVolume>(*World, FName(TEXT("BombSite"))).Num(), 2);
		Test.TestEqual("Two buy zones", FindActors<ATriggerVolume>(*World, FName(TEXT("BuyZone"))).Num(), 2);
		Test.TestEqual("Five CT starts", FindTeamStarts(*World, TEXT("CT")).Num(), 5);
		Test.TestEqual("Five T starts", FindTeamStarts(*World, TEXT("T")).Num(), 5);
		for (const APlayerStart* Start : FindTeamStarts(*World, TEXT("CT")))
		{
			Test.TestEqual("A CT start faces south", FMath::Abs(Start->GetActorRotation().Yaw), 180.0f, 0.01f);
			Test.TestEqual("On the floor, at the capsule's centre", Start->GetActorLocation().Z, 92.0f, 0.01f);
			Test.TestTrue(
				"A CT start out of mid's line", FMath::Abs(Start->GetActorLocation().Y) >= Expected.MinCTStartY);
		}
		const TArray<ANavigationWaypoint*> Waypoints = FindActors<ANavigationWaypoint>(*World);
		Test.TestTrue("A waypoint graph", Waypoints.Num() >= 10);
		int32 NumLinks = 0;
		int32 NumLookouts = 0;
		int32 NumClimbs = 0;
		for (const ANavigationWaypoint* Waypoint : Waypoints)
		{
			NumLinks += Waypoint->Links.Num();
			NumLookouts += Waypoint->HasFlag(TEXT("Lookout")) ? 1 : 0;
			for (const ANavigationWaypoint* Linked : Waypoint->Links)
			{
				Test.TestTrue("Links go both ways", Linked != nullptr && Linked->Links.Contains(Waypoint));
				// ps2-polish P3: each ladder's foot linked to its top (the import's link across the climb).
				NumClimbs += Linked != nullptr && Waypoint->HasFlag(TEXT("Ladder")) &&
						Linked->HasFlag(TEXT("Ladder")) &&
						Linked->GetActorLocation().Z - Waypoint->GetActorLocation().Z > 300.0f
					? 1
					: 0;
			}
		}
		Test.TestTrue("Linked", NumLinks >= 2 * Waypoints.Num() - 2);
		Test.TestEqual("The lookouts", NumLookouts, Expected.NumLookouts);
		Test.TestEqual("The ladders climbed", NumClimbs, Expected.NumClimbs);
		Test.TestEqual("The player clips", FindActors<ABlockingVolume>(*World).Num(), Expected.NumClips);
		Test.TestEqual(
			"The ladders", FindActors<ATriggerVolume>(*World, FName(TEXT("Ladder"))).Num(), Expected.NumLadders);
		Test.TestEqual("The sun", FindActors<ADirectionalLight>(*World).Num(), 1);
		return true;
	}

	/**
	 * A headless engine opens a map with the player's team (`?team=CT`, the team menu's choice): the player joins CT at
	 * a CT start, the nine bots join around it (4 CT, 5 T: AShooterGameMode::RebalanceBots), and after a second of play
	 * the ten pawns stand on ten different starts of their teams (the G6 smoke, in a test), on the floor slabs (N29),
	 * each on its spawn's surface. Returns the world (the engine still running) for the map's own checks, or null.
	 */
	UWorld* PlaceTenPawns(FAutomationTestBase& Test, UGameEngine& Engine, const TCHAR* MapName,
		EPhysicalSurface CTSpawnSurface, EPhysicalSurface TSpawnSurface, float CTSpawnMinX)
	{
		Engine.Init(nullptr);
		FWorldContext& Context = *Engine.GameInstance->GetWorldContext();
		FString Error;
		const FString URL = FString(MapName) + TEXT("?team=CT");
		if (!Test.TestEqual("Browse",
				static_cast<int32>(Engine.Browse(Context, FURL(nullptr, *URL, TRAVEL_Absolute), Error)),
				static_cast<int32>(EBrowseReturnVal::Success)))
		{
			Test.AddError(Error);
			return nullptr;
		}
		UWorld* World = Engine.GetGameWorld();
		AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
		APlayerController* Controller = Engine.GameInstance->GetFirstGamePlayer() != nullptr
			? Engine.GameInstance->GetFirstGamePlayer()->PlayerController
			: nullptr;
		if (!Test.TestNotNull("ShooterGameMode", GameMode) || !Test.TestNotNull("The player", Controller))
		{
			return nullptr;
		}
		const AShooterPlayerState* PlayerState = Controller->GetPlayerState<AShooterPlayerState>();
		Test.TestTrue("The player is CT", PlayerState != nullptr && PlayerState->GetTeam() == EShooterTeam::CT);

		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Engine.Tick(1.0f / 60.0f, false);
		}

		int32 NumCT = 0;
		int32 NumT = 0;
		GameMode->CountPawns(NumCT, NumT);
		Test.TestEqual("Five CT pawns", NumCT, 5);
		Test.TestEqual("Five T pawns", NumT, 5);
		TSet<const APlayerStart*> Used;
		for (const AShooterCharacter* Character : FindActors<AShooterCharacter>(*World))
		{
			const TArray<APlayerStart*> Starts =
				FindTeamStarts(*World, Character->GetTeam() == EShooterTeam::CT ? TEXT("CT") : TEXT("T"));
			const APlayerStart* Nearest = nullptr;
			for (const APlayerStart* Start : Starts)
			{
				const FVector Delta = Start->GetActorLocation() - Character->GetActorLocation();
				if (Delta.SizeSquared2D() < 1.0f)
				{
					Nearest = Start;
				}
			}
			Test.TestNotNull("Standing on a start of its team", Nearest);
			Test.TestEqual("On the ground", Character->GetActorLocation().Z, 0.0f, 0.5f);
			Test.TestTrue("Walking", Character->IsMovingOnGround());
			if (Nearest != nullptr)
			{
				Test.TestFalse("Alone on its start", Used.Contains(Nearest));
				Used.Add(Nearest);
			}
			const bool bCTSpawn = Character->GetActorLocation().X > CTSpawnMinX;
			Test.TestEqual(bCTSpawn ? TEXT("The CT spawn's floor") : TEXT("The T spawn's floor"),
				Character->GetFloorSurface(), bCTSpawn ? CTSpawnSurface : TSpawnSurface);
		}
		Test.TestEqual("Ten different starts", Used.Num(), 10);
		return World;
	}

	/** The surface a weapon trace from Start to End hits (its physical material's), SurfaceType_Max for none. */
	EPhysicalSurface SurfaceAlong(UWorld& World, const FVector& Start, const FVector& End)
	{
		FCollisionQueryParams Params;
		Params.bReturnPhysicalMaterial = true;
		FHitResult Hit;
		return World.GetPhysicsScene().LineTraceSingleByChannel(Hit, Start, End, COLLISION_WEAPON, Params)
			? UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get())
			: SurfaceType_Max;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapDeLeonHoldsTheGameTest, "ShooterGame.Map.DeLeonHoldsTheGame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapDeLeonHoldsTheGameTest::RunTest(const FString& Parameters)
{
	// de_leon: ps2-polish P3's three lookouts a site and two ladders to the roofs, the player clip at the low wall at
	// B; the CT starts out of the mid doors' line.
	TestEqual("The game mode", UGameMapsSettings::GetGlobalDefaultGameMode(),
		FString(TEXT("/Script/ShooterGame.ShooterGameMode")));
	return TestMapHoldsTheGame(*this, {DeLeon, 6, 2, 1, 2, 550.0f});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapDePuertoHoldsTheGameTest, "ShooterGame.Map.DePuertoHoldsTheGame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapDePuertoHoldsTheGameTest::RunTest(const FString& Parameters)
{
	// de_puerto, the second map: three lookouts a site, the ladder up the container stack at A, the quay's clip (nobody
	// falls into the water); the CT starts beside the courtyard's opening, out of mid's line.
	TestMapHoldsTheGame(*this, {DePuerto, 6, 1, 1, 1, 450.0f});
	const TArray<FString>& MapNames = GetDefault<UShooterMainMenuWidget>()->MapNames;
	if (TestEqual("The menu offers two maps", MapNames.Num(), 2))
	{
		TestEqual("de_leon first", MapNames[0], FString(DeLeon));
		TestEqual("de_puerto second", MapNames[1], FString(DePuerto));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapNavigationCoverageTest, "ShooterGame.Map.NavigationCoverage",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapNavigationCoverageTest::RunTest(const FString& Parameters)
{
	// Every map's waypoint graph is one piece (each waypoint reaches every other along the links, the ladders' climbs
	// included), and it covers what the bots go for: a waypoint within 3 m of each bomb site's middle and within 6 m of
	// each player start.
	for (const TCHAR* MapName : {DeLeon, DePuerto})
	{
		UPackage* Package = LoadPackage(nullptr, MapName, LOAD_None);
		UWorld* World = Package != nullptr ? UWorld::FindWorldInPackage(Package) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), MapName), World))
		{
			continue;
		}
		const TArray<ANavigationWaypoint*> Waypoints = FindActors<ANavigationWaypoint>(*World);
		if (!TestTrue(*FString::Printf(TEXT("%s: waypoints"), MapName), Waypoints.Num() > 0))
		{
			continue;
		}
		TSet<const ANavigationWaypoint*> Reached;
		TArray<const ANavigationWaypoint*> Open;
		Reached.Add(Waypoints[0]);
		Open.Add(Waypoints[0]);
		while (Open.Num() > 0)
		{
			const ANavigationWaypoint* Waypoint = Open.Pop();
			for (const ANavigationWaypoint* Linked : Waypoint->Links)
			{
				if (Linked != nullptr && !Reached.Contains(Linked))
				{
					Reached.Add(Linked);
					Open.Add(Linked);
				}
			}
		}
		TestEqual(*FString::Printf(TEXT("%s: every waypoint reached"), MapName), Reached.Num(), Waypoints.Num());
		auto NearestWaypoint = [&Waypoints](const FVector& Point)
		{
			float Nearest = TNumericLimits<float>::Max();
			for (const ANavigationWaypoint* Waypoint : Waypoints)
			{
				Nearest = FMath::Min(Nearest, FVector::Dist2D(Waypoint->GetActorLocation(), Point));
			}
			return Nearest;
		};
		for (const ATriggerVolume* Site : FindActors<ATriggerVolume>(*World, FName(TEXT("BombSite"))))
		{
			TestTrue(*FString::Printf(TEXT("%s: a waypoint at %s's middle"), MapName, *Site->GetName()),
				NearestWaypoint(Site->GetActorLocation()) <= 300.0f);
		}
		for (const APlayerStart* Start : FindActors<APlayerStart>(*World))
		{
			TestTrue(*FString::Printf(TEXT("%s: a waypoint near %s"), MapName, *Start->GetName()),
				NearestWaypoint(Start->GetActorLocation()) <= 600.0f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapRequiredTagsTest, "ShooterGame.Map.RequiredTags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapRequiredTagsTest::RunTest(const FString& Parameters)
{
	// The project's rules (DefaultEditor.ini): the maps' sources import and a map without the game's volumes and
	// starts (the engine's axes map) is refused, naming every missing entry.
	FScopedShooterTestContent Content;
	UObject* DeLeonMap = UImportAssetsCommandlet::ImportAsset(SourceArtFile(TEXT("SourceArt/Maps/de_leon.glb")),
		TEXT("/ShooterGameTest/Maps/de_leon"), FString(), TEXT("Map"), TMap<FString, FString>());
	TestNotNull("de_leon passes", DeLeonMap);
	UObject* DePuertoMap = UImportAssetsCommandlet::ImportAsset(SourceArtFile(TEXT("SourceArt/Maps/de_puerto.glb")),
		TEXT("/ShooterGameTest/Maps/de_puerto"), FString(), TEXT("Map"), TMap<FString, FString>());
	TestNotNull("de_puerto passes", DePuertoMap);

	for (const TCHAR* Entry :
		{TEXT("TriggerVolume:BombSite+A"), TEXT("TriggerVolume:BombSite+B"), TEXT("TriggerVolume:BuyZone+CT"),
			TEXT("TriggerVolume:BuyZone+T"), TEXT("PlayerStart:CT"), TEXT("PlayerStart:T")})
	{
		AddExpectedError(FString::Printf(TEXT("has nothing with the required tags '%s'"), Entry), 1);
	}
	AddExpectedError(TEXT("failed to import"), 1);
	const FString AxisTest =
		FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("SourceArt/Maps/AxisTest.glb")));
	TestNull("A map without bomb sites is refused",
		UImportAssetsCommandlet::ImportAsset(
			AxisTest, TEXT("/ShooterGameTest/Maps/AxisTest"), FString(), TEXT("Map"), TMap<FString, FString>()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapTenPawnsOnDeLeonTest, "ShooterGame.Map.TenPawnsOnDeLeon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapTenPawnsOnDeLeonTest::RunTest(const FString& Parameters)
{
	// de_leon's physical materials (N30f): the T spawn's sand (dirt), the CT spawn's paving (tile, from X 9 m); a crate
	// is wood, a wall concrete and a lamp of the tunnel metal to a bullet.
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	if (UWorld* World = PlaceTenPawns(*this, *Engine, DeLeon, SHOOTER_SURFACE_Tile, SHOOTER_SURFACE_Dirt, 900.0f))
	{
		TestEqual("A T spawn crate: wood",
			SurfaceAlong(*World, FVector(-2150.0f, 1100.0f, 300.0f), FVector(-2150.0f, 1100.0f, 0.0f)),
			SHOOTER_SURFACE_Wood);
		TestEqual("The T spawn's south wall: concrete",
			SurfaceAlong(*World, FVector(-2900.0f, 0.0f, 200.0f), FVector(-3100.0f, 0.0f, 200.0f)),
			SHOOTER_SURFACE_Concrete);
		TestEqual("A lamp of the tunnel: metal",
			SurfaceAlong(*World, FVector(-1200.0f, -1500.0f, 250.0f), FVector(-1200.0f, -1400.0f, 250.0f)),
			SHOOTER_SURFACE_Metal);
	}
	Engine->PreExit();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapTenPawnsOnDePuertoTest, "ShooterGame.Map.TenPawnsOnDePuerto",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapTenPawnsOnDePuertoTest::RunTest(const FString& Parameters)
{
	// de_puerto's surfaces: both spawns concrete (the T yard's asphalt, the CT yard's slabs); a pallet crate is wood, a
	// container metal and the boundary wall's panels concrete to a bullet.
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	if (UWorld* World =
			PlaceTenPawns(*this, *Engine, DePuerto, SHOOTER_SURFACE_Concrete, SHOOTER_SURFACE_Concrete, 2000.0f))
	{
		TestEqual("A T yard crate: wood",
			SurfaceAlong(*World, FVector(-2200.0f, 650.0f, 300.0f), FVector(-2200.0f, 650.0f, 0.0f)),
			SHOOTER_SURFACE_Wood);
		TestEqual("A T yard container: metal",
			SurfaceAlong(*World, FVector(-2880.0f, 1100.0f, 400.0f), FVector(-2880.0f, 1100.0f, 0.0f)),
			SHOOTER_SURFACE_Metal);
		TestEqual("The T yard's south wall: concrete",
			SurfaceAlong(*World, FVector(-3100.0f, 0.0f, 200.0f), FVector(-3300.0f, 0.0f, 200.0f)),
			SHOOTER_SURFACE_Concrete);
	}
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
