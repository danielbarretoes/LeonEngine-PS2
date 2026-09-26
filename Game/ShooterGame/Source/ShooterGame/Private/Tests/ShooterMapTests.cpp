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
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerState.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// de_leon and its rules: the imported map holds what the game needs, the project's RequiredTags reject a map without
// it, and a headless match on it places ten pawns at their teams' starts (gate G6 in a test).

namespace
{

	const TCHAR* const DeLeon = TEXT("/Game/Maps/de_leon");

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

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapDeLeonHoldsTheGameTest, "ShooterGame.Map.DeLeonHoldsTheGame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapDeLeonHoldsTheGameTest::RunTest(const FString& Parameters)
{
	// The imported blockout is the game's default map and holds two bomb sites, the teams' buy zones and five starts a
	// team, the waypoint graph with its links, a player clip and the sun.
	TestEqual("The default map", UGameMapsSettings::GetGameDefaultMap(), FString(DeLeon));
	TestEqual("The game mode", UGameMapsSettings::GetGlobalDefaultGameMode(),
		FString(TEXT("/Script/ShooterGame.ShooterGameMode")));
	UPackage* Package = LoadPackage(nullptr, DeLeon, LOAD_None);
	UWorld* World = Package != nullptr ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!TestNotNull("de_leon loads", World))
	{
		return false;
	}
	for (const TCHAR* Site : {TEXT("A"), TEXT("B")})
	{
		const TArray<ATriggerVolume*> Sites = FindActors<ATriggerVolume>(*World, FName(TEXT("BombSite")));
		bool bFound = false;
		for (const ATriggerVolume* Volume : Sites)
		{
			bFound |= Volume->ActorHasTag(FName(Site));
		}
		TestTrue(*FString::Printf(TEXT("Bomb site %s"), Site), bFound);
	}
	TestEqual("Two bomb sites", FindActors<ATriggerVolume>(*World, FName(TEXT("BombSite"))).Num(), 2);
	const TArray<ATriggerVolume*> BuyZones = FindActors<ATriggerVolume>(*World, FName(TEXT("BuyZone")));
	TestEqual("Two buy zones", BuyZones.Num(), 2);
	TestEqual("Five CT starts", FindTeamStarts(*World, TEXT("CT")).Num(), 5);
	TestEqual("Five T starts", FindTeamStarts(*World, TEXT("T")).Num(), 5);
	for (const APlayerStart* Start : FindTeamStarts(*World, TEXT("CT")))
	{
		TestEqual("A CT start faces south", FMath::Abs(Start->GetActorRotation().Yaw), 180.0f, 0.01f);
		TestEqual("On the floor, at the capsule's centre", Start->GetActorLocation().Z, 92.0f, 0.01f);
	}
	const TArray<ANavigationWaypoint*> Waypoints = FindActors<ANavigationWaypoint>(*World);
	TestTrue("A waypoint graph", Waypoints.Num() >= 10);
	int32 NumLinks = 0;
	for (const ANavigationWaypoint* Waypoint : Waypoints)
	{
		NumLinks += Waypoint->Links.Num();
		for (const ANavigationWaypoint* Linked : Waypoint->Links)
		{
			TestTrue("Links go both ways", Linked != nullptr && Linked->Links.Contains(Waypoint));
		}
	}
	TestTrue("Linked", NumLinks >= 2 * Waypoints.Num() - 2);
	TestEqual("A player clip", FindActors<ABlockingVolume>(*World).Num(), 1);
	TestEqual("The sun", FindActors<ADirectionalLight>(*World).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMapRequiredTagsTest, "ShooterGame.Map.RequiredTags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMapRequiredTagsTest::RunTest(const FString& Parameters)
{
	// The project's rules (DefaultEditor.ini): de_leon's source imports and a map without the game's volumes and
	// starts (the engine's axes map) is refused, naming every missing entry.
	FScopedShooterTestContent Content;
	UObject* DeLeonMap = UImportAssetsCommandlet::ImportAsset(SourceArtFile(TEXT("SourceArt/Maps/de_leon.glb")),
		TEXT("/ShooterGameTest/Maps/de_leon"), FString(), TEXT("Map"), TMap<FString, FString>());
	TestNotNull("de_leon passes", DeLeonMap);

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
	// A headless engine opens the default map as the game does: the player joins CT at a CT start, bot_add_ct 4 and
	// bot_add_t 5 fill both teams, and after a second of play the ten pawns stand on ten different starts of their
	// teams (the G6 smoke, in a test).
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	if (!TestEqual("Browse", static_cast<int32>(Engine->Browse(Context, FURL(nullptr, DeLeon, TRAVEL_Absolute), Error)),
			static_cast<int32>(EBrowseReturnVal::Success)))
	{
		AddError(Error);
		Engine->PreExit();
		return false;
	}
	UWorld* World = Engine->GetGameWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer() != nullptr
		? Engine->GameInstance->GetFirstGamePlayer()->PlayerController
		: nullptr;
	if (!TestNotNull("ShooterGameMode", GameMode) || !TestNotNull("The player", Controller))
	{
		Engine->PreExit();
		return false;
	}
	const AShooterPlayerState* PlayerState = Controller->GetPlayerState<AShooterPlayerState>();
	TestTrue("The player is CT", PlayerState != nullptr && PlayerState->GetTeam() == EShooterTeam::CT);

	TestEqual("Four CT bots", GameMode->AddBots(EShooterTeam::CT, 4), 4);
	TestEqual("Five T bots", GameMode->AddBots(EShooterTeam::T, 5), 5);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Engine->Tick(1.0f / 60.0f, false);
	}

	int32 NumCT = 0;
	int32 NumT = 0;
	GameMode->CountPawns(NumCT, NumT);
	TestEqual("Five CT pawns", NumCT, 5);
	TestEqual("Five T pawns", NumT, 5);
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
		TestNotNull("Standing on a start of its team", Nearest);
		TestEqual("On the floor (the 1 cm spawn pad)", Character->GetActorLocation().Z, 1.0f, 0.5f);
		TestTrue("Walking", Character->IsMovingOnGround());
		if (Nearest != nullptr)
		{
			TestFalse("Alone on its start", Used.Contains(Nearest));
			Used.Add(Nearest);
		}
	}
	TestEqual("Ten different starts", Used.Num(), 10);
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
