#include "Camera/CameraActor.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** The class the project's GlobalDefaultGameMode names (BaseEngine.ini). */
	UClass* GlobalDefaultGameModeClass()
	{
		return LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
	}

	/** The game mode class the game instance picks for URL in a fresh world, whose settings name WorldGameMode. */
	UClass* PickGameMode(const TCHAR* URLText, UClass* WorldGameMode)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AWorldSettings* Settings = World.SpawnActor<AWorldSettings>();
		World.PersistentLevel->SetWorldSettings(Settings);
		Settings->DefaultGameMode = WorldGameMode;
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		AGameModeBase* GameMode = GameInstance->CreateGameModeForURL(FURL(nullptr, URLText, TRAVEL_Absolute), &World);
		return GameMode != nullptr ? GameMode->GetClass() : nullptr;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FURLParsesMapOptionsAndPortalTest, "System.Engine.URL.ParsesMapOptionsAndPortal",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FURLParsesMapOptionsAndPortalTest::RunTest(const FString& Parameters)
{
	// UE's URL form: Map?Option?Key=Value#Portal; a partial travel keeps the base's options, a relative one its map.
	const FURL URL(nullptr, TEXT("/Game/Maps/Arena?game=/Script/Engine.GameMode?Name=Bob#Red"), TRAVEL_Absolute);
	TestEqual("Map", URL.Map, FString(TEXT("/Game/Maps/Arena")));
	TestEqual("Two options", URL.Op.Num(), 2);
	TestEqual("Game option", FString(URL.GetOption(TEXT("game="), TEXT(""))), FString(TEXT("/Script/Engine.GameMode")));
	TestTrue("Has Name", URL.HasOption(TEXT("Name")));
	TestFalse("No Listen", URL.HasOption(TEXT("Listen")));
	TestEqual("Portal", URL.Portal, FString(TEXT("Red")));
	TestEqual(
		"Round trip", URL.ToString(), FString(TEXT("/Game/Maps/Arena?game=/Script/Engine.GameMode?Name=Bob#Red")));

	const FURL File(nullptr, TEXT("C:/Content/Maps/Test.lmap?listen"), TRAVEL_Absolute);
	TestEqual("File name map", File.Map, FString(TEXT("C:/Content/Maps/Test.lmap")));
	TestTrue("File option", File.HasOption(TEXT("listen")));

	const FURL Base(nullptr, TEXT("/Game/A?Quiet?Difficulty=2#Start"), TRAVEL_Absolute);
	const FURL Partial(&Base, TEXT("/Game/B"), TRAVEL_Partial);
	TestEqual("Partial map", Partial.Map, FString(TEXT("/Game/B")));
	TestEqual("Partial keeps the options but Quiet", Partial.ToString(), FString(TEXT("/Game/B?Difficulty=2")));
	const FURL Relative(&Base, TEXT("?Name=X"), TRAVEL_Relative);
	TestEqual("Relative", Relative.ToString(), FString(TEXT("/Game/A?Difficulty=2?Name=X#Start")));

	const FURL Empty(nullptr, TEXT(""), TRAVEL_Absolute);
	TestEqual("No map: the default map", Empty.Map, UGameMapsSettings::GetGameDefaultMap());

	FURL Edited(nullptr, TEXT("/Game/A?Name=A"), TRAVEL_Absolute);
	Edited.AddOption(TEXT("name=B"));
	Edited.AddOption(TEXT("Listen"));
	TestEqual("Replaced and added", Edited.ToString(), FString(TEXT("/Game/A?name=B?Listen")));
	Edited.RemoveOption(TEXT("Name"));
	TestEqual("Removed", Edited.ToString(), FString(TEXT("/Game/A?Listen")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineSettingsGameMapsSettingsFromConfigTest,
	"System.Engine.EngineSettings.GameMapsSettingsFromConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineSettingsGameMapsSettingsFromConfigTest::RunTest(const FString& Parameters)
{
	// [/Script/EngineSettings.GameMapsSettings] of BaseEngine.ini: the default template map (a .lmap under /Engine),
	// the global default game mode and the game instance class; the game engine class of [/Script/Engine.Engine].
	TestEqual("GameDefaultMap", UGameMapsSettings::GetGameDefaultMap(), FString(TEXT("/Engine/Maps/Template_Default")));
	TestTrue("GameDefaultMap is a .lmap under /Engine",
		FPaths::FileExists(FPackageName::LongPackageNameToFilename(
			UGameMapsSettings::GetGameDefaultMap(), FPackageName::GetMapPackageExtension())));
	TestEqual("GlobalDefaultGameMode", UGameMapsSettings::GetGlobalDefaultGameMode(),
		FString(TEXT("/Script/Engine.GameModeBase")));
	TestNotNull("GlobalDefaultGameMode loads", GlobalDefaultGameModeClass());
	TestTrue("GameInstanceClass",
		GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>() ==
			UGameInstance::StaticClass());
	TestEqual("An unknown alias is the name itself",
		UGameMapsSettings::GetGameModeForName(TEXT("/Script/Engine.GameMode")),
		FString(TEXT("/Script/Engine.GameMode")));

	FString GameEngineClassName;
	GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), GameEngineClassName, GEngineIni);
	TestTrue("GameEngine class",
		StaticLoadClass(UEngine::StaticClass(), nullptr, *GameEngineClassName) == UGameEngine::StaticClass());
	TestTrue("LocalPlayerClassName",
		GetDefault<UGameEngine>()->LocalPlayerClassName.TryLoadClass<ULocalPlayer>() == ULocalPlayer::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadMapGameModePrecedenceTest, "System.Engine.LoadMap.GameModePrecedence",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoadMapGameModePrecedenceTest::RunTest(const FString& Parameters)
{
	// Plan decision D18: ?game= in the URL, then the level's world settings, then GlobalDefaultGameMode.
	UClass* GlobalDefault = GlobalDefaultGameModeClass();
	TestTrue("Neither: the global default", PickGameMode(TEXT("/Game/Maps/X"), nullptr) == GlobalDefault);
	TestTrue(
		"The world settings", PickGameMode(TEXT("/Game/Maps/X"), AGameMode::StaticClass()) == AGameMode::StaticClass());
	TestTrue("?game= over the world settings",
		PickGameMode(TEXT("/Game/Maps/X?game=/Script/Engine.GameModeBase"), AGameMode::StaticClass()) ==
			AGameModeBase::StaticClass());
	TestTrue("?game= over the global default",
		PickGameMode(TEXT("/Game/Maps/X?game=/Script/Engine.GameMode"), nullptr) == AGameMode::StaticClass());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadMapStarterLogsInThePlayerTest, "System.Engine.LoadMap.StarterLogsInThePlayer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoadMapStarterLogsInThePlayerTest::RunTest(const FString& Parameters)
{
	// A headless engine opens the default template map (the Starter level migrated to a .lmap): its world with the
	// global default game mode, the local player logged in with its pawn at the map's first player start (the view the
	// legacy camera framing opened with), and play begun. ?game= opens it again with another game mode (the old world
	// goes); a missing map fails and keeps the world.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
	if (!FPackageName::DoesPackageExist(StarterMap))
	{
		AddError(TEXT("Template_Default.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	const TWeakObjectPtr<UWorld> Placeholder = Context.World();
	FString Error;
	if (!TestEqual("Browse",
			static_cast<int32>(Engine->Browse(Context, FURL(nullptr, *StarterMap, TRAVEL_Absolute), Error)),
			static_cast<int32>(EBrowseReturnVal::Success)))
	{
		AddError(Error);
		Engine->PreExit();
		return false;
	}
	UWorld* World = Engine->GetGameWorld();
	TestTrue("A new world", World != nullptr && !Placeholder.IsValid());
	TestTrue("Plays", World->HasBegunPlay());
	TestTrue("Global default game mode", World->GetAuthGameMode()->GetClass() == GlobalDefaultGameModeClass());
	TestEqual("Last URL", Context.LastURL.Map, StarterMap);

	ULocalPlayer* Player = Engine->GameInstance->GetFirstGamePlayer();
	APlayerController* Controller = Player != nullptr ? Player->PlayerController : nullptr;
	if (!TestNotNull("Logged in", Controller))
	{
		Engine->PreExit();
		return false;
	}
	TestTrue("The controller's player", Controller->Player == Player);
	TestTrue("In the game state", World->GetGameState()->GetNumPlayers() == 1);
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(*World, APlayerStart::StaticClass(), Starts);
	if (TestEqual("The framing start and the level's", Starts.Num(), 2) && TestNotNull("A pawn", Controller->GetPawn()))
	{
		// The first player start is the view the legacy framing (the map's camera actor) opened with: at its eye,
		// looking at its target.
		const AActor* Start = Starts[0];
		const ACameraActor* Framing = World->FindFirst<ACameraActor>();
		if (TestNotNull("The framing camera", Framing))
		{
			const UCameraComponent& Camera = *Framing->GetCameraComponent();
			TestTrue("The first start is at the framing's eye",
				Start->GetActorLocation().Equals(Camera.GetCameraLocation(), 1.0e-3f));
			TestTrue("It looks at the framing's target",
				Start->GetActorRotation().Vector().Equals(
					(Camera.GetTarget() - Camera.GetCameraLocation()).GetSafeNormal(), 1.0e-4f));
		}
		TestTrue(
			"Pawn at the start", Controller->GetPawn()->GetActorLocation().Equals(Start->GetActorLocation(), 0.0f));
		TestTrue("Pawn turned to the start's yaw",
			FMath::IsNearlyEqual(
				Controller->GetPawn()->GetActorRotation().Yaw, Start->GetActorRotation().Yaw, 1.0e-3f));
		TestTrue("Control rotation is the start's",
			Controller->GetControlRotation().Equals(Start->GetActorRotation(), 0.0f));
		TestTrue("Start spot", Controller->StartSpot.Get() == Start);
	}

	TWeakObjectPtr<UWorld> OldWorld = World;
	const FURL WithGame(nullptr, *(StarterMap + TEXT("?game=/Script/Engine.GameMode")), TRAVEL_Absolute);
	TestEqual("Browse again", static_cast<int32>(Engine->Browse(Context, WithGame, Error)),
		static_cast<int32>(EBrowseReturnVal::Success));
	TestFalse("The old world is gone", OldWorld.IsValid());
	World = Engine->GetGameWorld();
	TestTrue("?game= game mode", World->GetAuthGameMode()->GetClass() == AGameMode::StaticClass());
	TestNotNull("Logged in again", Player->PlayerController);
	TestEqual("Two maps opened", Engine->GameInstance->GetLevelsOpened(), 2);

	TestEqual("Missing map",
		static_cast<int32>(Engine->Browse(Context, FURL(nullptr, TEXT("/Engine/NoSuchMap"), TRAVEL_Absolute), Error)),
		static_cast<int32>(EBrowseReturnVal::Failure));
	TestFalse("An error", Error.IsEmpty());
	TestTrue("The world stays", Engine->GetGameWorld() == World);
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
