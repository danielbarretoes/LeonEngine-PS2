#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterGame_Menu.h"
#include "ShooterHUD.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerController_Menu.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "UI/ShooterMainMenuWidget.h"
#include "UI/ShooterPauseMenuWidget.h"
#include "UI/ShooterTeamMenuWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-polish P9: the main menu's match (its URL options and what the game mode makes of them), the bots' difficulty
// presets, the bots shared out around the player's team, the team change at the next round, the pause, and the travel
// between the main menu and a match.

namespace
{
	constexpr float FrameTime = 1.0f / 60.0f;
	const TCHAR* const DeLeon = TEXT("/Game/Maps/de_leon");
	const TCHAR* const MainMenuMap = TEXT("/Game/Maps/MainMenu");

	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	/**
	 * A small match map (a floor, five starts a team with their buy zones, the world settings a pause needs) and its
	 * game mode, with short phases; no bots.
	 */
	AShooterGameMode* SetUpMatch(UWorld& World)
	{
		AWorldSettings* Settings = World.SpawnActor<AWorldSettings>();
		World.PersistentLevel->SetWorldSettings(Settings);
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = (static_cast<float>(Index) - 2.0f) * 150.0f;
			APlayerStart* CTStart = World.SpawnActor<APlayerStart>(FVector(-1500.0f, Y, 92.0f), FRotator::ZeroRotator);
			CTStart->PlayerStartTag = FName(TEXT("CT"));
			APlayerStart* TStart =
				World.SpawnActor<APlayerStart>(FVector(1500.0f, Y, 92.0f), FRotator(0.0f, 180.0f, 0.0f));
			TStart->PlayerStartTag = FName(TEXT("T"));
		}
		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->bBotStop = true;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 20.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->MaxRounds = 30;
		return GameMode;
	}

	/** A local human player (its HUD, the menus' home) that joined the match. */
	AShooterPlayerController* AddHuman(UWorld& World, AShooterGameMode& GameMode)
	{
		AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
		Player->SetPlayer(NewObject<ULocalPlayer>(Player));
		GameMode.PostLogin(Player);
		return Player;
	}

	/** The bots of a team. */
	int32 CountBots(const AShooterGameMode& GameMode, EShooterTeam Team)
	{
		int32 Count = 0;
		for (const APlayerState* State : GameMode.GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
			Count += ShooterState != nullptr && ShooterState->bIsABot && ShooterState->GetTeam() == Team ? 1 : 0;
		}
		return Count;
	}

	void TickUntilLive(UWorld& World, const AShooterGameMode& GameMode)
	{
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const AShooterGameState* State = GameMode.GetShooterGameState();
			if (State != nullptr && State->GetRoundState() == EShooterRoundState::Live)
			{
				return;
			}
			World.Tick(FrameTime);
		}
	}

	/** Paints the player's HUD into a 640 x 448 frame (the widgets' focus navigation uses the last paint). */
	void PaintHUD(APlayerController& Player)
	{
		if (Player.MyHUD != nullptr)
		{
			FCanvas Canvas(640, 448);
			Player.MyHUD->Paint(Canvas);
		}
	}

	/** Presses and releases a key through the controller (its HUD's widgets first), painting as a frame does. */
	void Tap(UWorld& World, APlayerController& Player, const FKey& Key, bool bGamepad)
	{
		PaintHUD(Player);
		(void)Player.InputKey(Key, IE_Pressed, 1.0f, bGamepad);
		TickFrames(World, 1);
		(void)Player.InputKey(Key, IE_Released, 0.0f, bGamepad);
		TickFrames(World, 1);
		PaintHUD(Player);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuMatchOptionsTest, "ShooterGame.Menu.MatchOptions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuMatchOptionsTest::RunTest(const FString& Parameters)
{
	// The menu's choices become URL options, and InitGame makes them the match's: the bots, their difficulty, and
	// MaxRounds = 2 N - 1 for N rounds to win (the halftime at MaxRounds / 2).
	FShooterMatchSettings Settings;
	Settings.NumBots = 4;
	Settings.BotDifficulty = EShooterBotDifficulty::Hard;
	Settings.RoundsToWin = 5;
	TestEqual("The URL options", Settings.GetURLOptions(), FString(TEXT("bots=4?difficulty=Hard?winrounds=5")));
	TestEqual("Best of 5", FShooterMatchSettings::GetMaxRounds(3), 5);
	TestEqual("Best of 31", FShooterMatchSettings::GetMaxRounds(16), 31);
	EShooterBotDifficulty Parsed = EShooterBotDifficulty::Normal;
	TestTrue("A name, any case", ParseBotDifficulty(TEXT("expert"), Parsed) && Parsed == EShooterBotDifficulty::Expert);
	TestTrue("CS's number", ParseBotDifficulty(TEXT("0"), Parsed) && Parsed == EShooterBotDifficulty::Easy);
	TestFalse("Not a difficulty", ParseBotDifficulty(TEXT("Nightmare"), Parsed));

	struct FCase
	{
		int32 RoundsToWin;
		int32 MaxRounds;
		int32 Halftime;
	};
	for (const FCase& Case : {FCase{3, 5, 2}, FCase{5, 9, 4}, FCase{8, 15, 7}, FCase{16, 31, 15}})
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpMatch(World);
		Settings.RoundsToWin = Case.RoundsToWin;
		FString Error;
		GameMode->InitGame(TEXT("de_leon"), TEXT("?") + Settings.GetURLOptions(), Error);
		TestEqual("The bots", GameMode->NumBots, 4);
		TestTrue("Hard", GameMode->BotDifficulty == EShooterBotDifficulty::Hard);
		TestEqual("MaxRounds = 2 N - 1", GameMode->MaxRounds, Case.MaxRounds);
		TestEqual("N rounds win", GameMode->GetRoundsToWin(), Case.RoundsToWin);
		TestEqual("The halftime", GameMode->GetHalftimeRound(), Case.Halftime);
	}

	// Without options: the config's match (nine bots, Normal, 30 rounds).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World);
	FString Error;
	GameMode->InitGame(TEXT("de_leon"), TEXT("?difficulty=Nightmare"), Error);
	TestEqual("Nine bots by default", GameMode->NumBots, 9);
	TestTrue("A bad difficulty keeps Normal", GameMode->BotDifficulty == EShooterBotDifficulty::Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuDifficultyPresetsTest, "ShooterGame.Menu.DifficultyPresets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuDifficultyPresetsTest::RunTest(const FString& Parameters)
{
	// DefaultGame.ini has a preset a difficulty, Normal the bots' old skill; a bot the game mode adds takes its
	// match's preset, and a harder one reacts sooner, aims better, turns faster and remembers longer.
	const AShooterAIController* Defaults = GetDefault<AShooterAIController>();
	TestEqual("Four presets", Defaults->DifficultyPresets.Num(), 4);
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World);
	float LastReaction = 1.0e6f;
	float LastAimError = 1.0e6f;
	float LastTurnRate = 0.0f;
	float LastMemory = 0.0f;
	for (const EShooterBotDifficulty Difficulty : {EShooterBotDifficulty::Easy, EShooterBotDifficulty::Normal,
			 EShooterBotDifficulty::Hard, EShooterBotDifficulty::Expert})
	{
		GameMode->BotDifficulty = Difficulty;
		const int32 Before = GameMode->GetGameState().GetPlayerArray().Num();
		TestEqual("A bot joins", GameMode->AddBots(EShooterTeam::CT, 1), 1);
		const APlayerState* State = GameMode->GetGameState().GetPlayerArray()[Before];
		const AShooterAIController* Bot = State != nullptr ? Cast<AShooterAIController>(State->GetOwner()) : nullptr;
		if (!TestNotNull("The bot", Bot))
		{
			return false;
		}
		const FShooterBotSkill* Preset = Defaults->DifficultyPresets.FindByPredicate(
			[Difficulty](const FShooterBotSkill& Skill) { return Skill.Difficulty == Difficulty; });
		if (!TestNotNull(GetBotDifficultyName(Difficulty), Preset))
		{
			return false;
		}
		TestTrue("It took the difficulty", Bot->GetDifficulty() == Difficulty);
		TestEqual("Its reaction", Bot->ReactionTime, Preset->ReactionTime);
		TestEqual("Its aim error", Bot->AimError, Preset->AimError);
		TestEqual("Its turn rate", Bot->AimTurnRate, Preset->AimTurnRate);
		TestEqual("Its recoil control", Bot->RecoilCompensation, Preset->RecoilCompensation);
		TestEqual("Its memory", Bot->EnemyMemory, Preset->EnemyMemory);
		TestTrue("Harder reacts sooner", Bot->ReactionTime < LastReaction);
		TestTrue("aims better", Bot->AimError < LastAimError);
		TestTrue("turns faster", Bot->AimTurnRate > LastTurnRate);
		TestTrue("remembers longer", Bot->EnemyMemory > LastMemory);
		LastReaction = Bot->ReactionTime;
		LastAimError = Bot->AimError;
		LastTurnRate = Bot->AimTurnRate;
		LastMemory = Bot->EnemyMemory;
		if (Difficulty == EShooterBotDifficulty::Normal)
		{
			// Normal is the skill the bots had before the presets (the BotMatch replays as before).
			TestEqual("Normal: the class's reaction", Bot->ReactionTime, Defaults->ReactionTime);
			TestEqual("Normal: the class's aim", Bot->AimError, Defaults->AimError);
			TestEqual("Normal: the class's turn rate", Bot->AimTurnRate, Defaults->AimTurnRate);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuBotSplitTest, "ShooterGame.Menu.BotSplit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuBotSplitTest::RunTest(const FString& Parameters)
{
	// The bots even the teams out counting the player: half of all the players a side, the odd one to the player's
	// opponents (T when nobody plays), never more than five a side.
	struct FCase
	{
		int32 Bots;
		int32 HumansCT;
		int32 HumansT;
		int32 BotsCT;
		int32 BotsT;
	};
	const FCase Cases[] = {{9, 1, 0, 4, 5}, {9, 0, 1, 5, 4}, {4, 1, 0, 1, 3}, {5, 1, 0, 2, 3}, {1, 1, 0, 0, 1},
		{1, 0, 1, 1, 0}, {2, 1, 0, 0, 2}, {9, 0, 0, 4, 5}, {10, 0, 0, 5, 5}, {0, 1, 0, 0, 0}, {12, 1, 0, 4, 5},
		{3, 0, 0, 1, 2}};
	for (const FCase& Case : Cases)
	{
		int32 BotsCT = -1;
		int32 BotsT = -1;
		AShooterGameMode::ComputeBotSplit(Case.Bots, Case.HumansCT, Case.HumansT, 5, BotsCT, BotsT);
		const FString What =
			FString::Printf(TEXT("%d bot(s), %d CT and %d T player(s)"), Case.Bots, Case.HumansCT, Case.HumansT);
		TestEqual(*(What + TEXT(": CT bots")), BotsCT, Case.BotsCT);
		TestEqual(*(What + TEXT(": T bots")), BotsT, Case.BotsT);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuTeamChoiceTest, "ShooterGame.Menu.TeamChoiceAndChange",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuTeamChoiceTest::RunTest(const FString& Parameters)
{
	// The player's first choice (CT) lets nine bots join around it: 4 CT and 5 T, and the match starts. A change to T
	// during the live round kills the player (a death) and makes it a terrorist from the next round, where one bot
	// moves over to CT: 5 CT bots, 4 T bots and the player.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World);
	GameMode->NumBots = 9;
	AShooterPlayerController* Player = AddHuman(World, *GameMode);
	AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player's state", PlayerState))
	{
		return false;
	}
	TickFrames(World, 3);
	TestEqual(
		"No bot before the choice", CountBots(*GameMode, EShooterTeam::CT) + CountBots(*GameMode, EShooterTeam::T), 0);
	TestFalse("The game waits", GameMode->IsMatchInProgress());

	TestTrue("CT", GameMode->SelectTeam(Player, EShooterTeamChoice::CT));
	TickFrames(World, 2);
	TestTrue("A counter-terrorist", PlayerState->GetTeam() == EShooterTeam::CT);
	TestEqual("Four CT bots", CountBots(*GameMode, EShooterTeam::CT), 4);
	TestEqual("Five T bots", CountBots(*GameMode, EShooterTeam::T), 5);
	TestTrue("The match started", GameMode->IsMatchInProgress());
	TestFalse("The same team again changes nothing", GameMode->SelectTeam(Player, EShooterTeamChoice::CT));

	TickUntilLive(World, *GameMode);
	const AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	TestTrue("It plays", Pawn != nullptr && Pawn->IsAlive());
	const int32 Deaths = PlayerState->GetDeaths();
	TestTrue("T", GameMode->SelectTeam(Player, EShooterTeamChoice::T));
	TestTrue("It died (CS)", Pawn == nullptr || !Pawn->IsAlive());
	TestEqual("A death on the board", PlayerState->GetDeaths(), Deaths + 1);
	TestTrue("A terrorist now", PlayerState->GetTeam() == EShooterTeam::T);
	TestEqual("The bots wait for the next round: still 4 CT", CountBots(*GameMode, EShooterTeam::CT), 4);

	const AShooterGameState* State = GameMode->GetShooterGameState();
	const int32 Round = State->GetRoundNumber();
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	TickFrames(World, FMath::CeilToInt(GameMode->RoundRestartDelay / FrameTime) + 2);
	TestEqual("The next round", State->GetRoundNumber(), Round + 1);
	TestEqual("Five CT bots", CountBots(*GameMode, EShooterTeam::CT), 5);
	TestEqual("Four T bots", CountBots(*GameMode, EShooterTeam::T), 4);
	const AShooterCharacter* NewPawn = Cast<AShooterCharacter>(Player->GetPawn());
	TestTrue(
		"The player plays for T", NewPawn != nullptr && NewPawn->IsAlive() && NewPawn->GetTeam() == EShooterTeam::T);
	TestTrue("On the T side", NewPawn != nullptr && NewPawn->GetActorLocation().X > 1000.0f);
	int32 NumCT = 0;
	int32 NumT = 0;
	GameMode->CountPawns(NumCT, NumT);
	TestEqual("Five a side: CT", NumCT, 5);
	TestEqual("Five a side: T", NumT, 5);

	// Spectate: the player leaves the sides and the nine bots share them out again (4 CT, 5 T).
	TestTrue("Spectate", GameMode->SelectTeam(Player, EShooterTeamChoice::Spectate));
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	TickFrames(World, FMath::CeilToInt(GameMode->RoundRestartDelay / FrameTime) + 2);
	TestTrue("No team", PlayerState->GetTeam() == EShooterTeam::None);
	TestEqual("Spectating: four CT bots", CountBots(*GameMode, EShooterTeam::CT), 4);
	TestEqual("and five T", CountBots(*GameMode, EShooterTeam::T), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuPauseTest, "ShooterGame.Menu.PauseMenu",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuPauseTest::RunTest(const FString& Parameters)
{
	// Start opens the pause menu and pauses the match: the world's time, the round's clock and its timers stop, the
	// bots stand, but the HUD's menu ticks (its match line) and takes the keys; Escape resumes. The Change team line
	// swaps it for the team menu (still paused), whose Back returns to it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World);
	GameMode->NumBots = 3;
	GameMode->bBotStop = false;
	AShooterPlayerController* Player = AddHuman(World, *GameMode);
	(void)GameMode->SelectTeam(Player, EShooterTeamChoice::CT);
	TickUntilLive(World, *GameMode);
	TickFrames(World, 30);
	const AShooterGameState* State = GameMode->GetShooterGameState();
	if (!TestNotNull("The HUD", Cast<AShooterHUD>(Player->MyHUD)) ||
		!TestTrue("Live", State->GetRoundState() == EShooterRoundState::Live))
	{
		return false;
	}

	Tap(World, *Player, EKeys::Gamepad_Special_Right, true);
	UShooterPauseMenuWidget* Menu = Player->GetPauseMenu();
	if (!TestTrue("Start opens the pause menu", Menu != nullptr && Player->IsPauseMenuOpen()))
	{
		return false;
	}
	TestTrue("and pauses the game", World.IsPaused() && UGameplayStatics::IsGamePaused(&World));
	const float Time = World.GetTimeSeconds();
	const float PhaseEnd = State->GetPhaseEndTime();
	TArray<FVector> BotPlaces;
	for (const AShooterCharacter* Pawn : GameMode->GetPawns())
	{
		BotPlaces.Add(Pawn->GetActorLocation());
	}
	TickFrames(World, 120);
	TestEqual("The world's time stops", World.GetTimeSeconds(), Time);
	TestEqual("and the round's clock", State->GetPhaseEndTime(), PhaseEnd);
	TestTrue("Still live (the timers stopped)", State->GetRoundState() == EShooterRoundState::Live);
	for (int32 Index = 0; Index < BotPlaces.Num() && Index < GameMode->GetPawns().Num(); ++Index)
	{
		TestTrue("The pawns stand", GameMode->GetPawns()[Index]->GetActorLocation().Equals(BotPlaces[Index], 0.01f));
	}
	TestTrue("The menu ticks while paused (its match line)", Menu->GetStatusText().Contains(TEXT("round")));
	TestTrue("It has the focus on Resume", Menu->GetResumeButton()->HasKeyboardFocus());

	// The D-pad's down reaches Change team, and Cross takes it: the team menu.
	Tap(World, *Player, EKeys::Gamepad_DPad_Down, true);
	TestTrue("The D-pad moves the focus", Menu->GetChangeTeamButton()->HasKeyboardFocus());
	Tap(World, *Player, EKeys::Gamepad_FaceButton_Bottom, true);
	TestTrue("Change team: the team menu", Player->IsTeamMenuOpen() && !Player->IsPauseMenuOpen());
	TestTrue("A change, not the first choice", !Player->GetTeamMenu()->IsInitialChoice());
	TestTrue("still paused", World.IsPaused());
	Tap(World, *Player, EKeys::Escape, false);
	TestTrue("Back: the pause menu again", Player->IsPauseMenuOpen() && !Player->IsTeamMenuOpen());
	Tap(World, *Player, EKeys::Escape, false);
	TestFalse("Escape resumes", Player->IsPauseMenuOpen() || World.IsPaused());
	TickFrames(World, 3);
	TestTrue("The time goes on", World.GetTimeSeconds() > Time);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMenuTravelTest, "ShooterGame.Menu.MainMenuToMatchAndBack",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMenuTravelTest::RunTest(const FString& Parameters)
{
	// The game opens the main menu (GameDefaultMap, its game mode by the map's prefix): no pawn, the main menu shown.
	// Its choices travel to de_leon as URL options, where the player waits in the team menu; Quit to main menu goes
	// back.
	TestEqual("The default map", UGameMapsSettings::GetGameDefaultMap(), FString(MainMenuMap));
	if (!FPackageName::DoesPackageExist(MainMenuMap))
	{
		AddError(TEXT("MainMenu.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	UEngine* const SavedEngine = GEngine;
	GEngine = Engine.Get();
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	(void)Engine->Browse(Context, FURL(nullptr, MainMenuMap, TRAVEL_Absolute), Error);
	UWorld* World = Engine->GetGameWorld();
	AShooterPlayerController_Menu* MenuPlayer =
		Cast<AShooterPlayerController_Menu>(Engine->GameInstance->GetFirstGamePlayer()->PlayerController);
	const bool bMenu = TestNotNull("The menu's world", World) &&
		TestNotNull("The menu's game mode", World->GetAuthGameMode<AShooterGame_Menu>()) &&
		TestNotNull("The menu's player", MenuPlayer) && TestNotNull("The main menu", MenuPlayer->GetMainMenu());
	if (!bMenu)
	{
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	TestNull("No pawn", MenuPlayer->GetPawn());
	UShooterMainMenuWidget* MainMenu = MenuPlayer->GetMainMenu();
	TestTrue("Shown", MainMenu->IsShown());
	TestEqual("de_leon first", MainMenu->GetMatchSettings().MapName, FString(DeLeon));
	// Four bots (the default nine, down five), Expert (two up from Normal), the first to 5.
	for (int32 Step = 0; Step < 5; ++Step)
	{
		MainMenu->GetBotsButton()->OnValueStep.Broadcast(-1);
	}
	MainMenu->StepDifficulty(1);
	MainMenu->StepDifficulty(1);
	MainMenu->StepRoundsToWin(1);
	const FShooterMatchSettings& Settings = MainMenu->GetMatchSettings();
	TestEqual("Four bots", Settings.NumBots, 4);
	TestTrue("Expert", Settings.BotDifficulty == EShooterBotDifficulty::Expert);
	TestEqual("First to 5", Settings.RoundsToWin, 5);
	MainMenu->StartMatch();
	TestEqual("Start: the travel waits for the next frame", Context.TravelURL,
		FString(DeLeon) + TEXT("?bots=4?difficulty=Expert?winrounds=5"));
	TestTrue("The choices are the player's", MenuPlayer->GetPersistentUser()->GetMatchSettings().NumBots == 4);

	Engine->TickWorldTravel(Context, 0.0f);
	World = Engine->GetGameWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	AShooterPlayerController* Player =
		Cast<AShooterPlayerController>(Engine->GameInstance->GetFirstGamePlayer()->PlayerController);
	if (!TestNotNull("de_leon's game mode", GameMode) || !TestNotNull("The player", Player))
	{
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	TestEqual("Four bots", GameMode->NumBots, 4);
	TestTrue("Expert", GameMode->BotDifficulty == EShooterBotDifficulty::Expert);
	TestEqual("A best of 9", GameMode->MaxRounds, 9);
	TestTrue("The player chooses its team", GameMode->IsChoosingTeam(Player) && Player->IsTeamMenuOpen());
	TestTrue("The first choice", Player->GetTeamMenu()->IsInitialChoice());
	TestFalse("The warmup is not paused", World->IsPaused());
	Player->JoinTeam(TEXT("T"));
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		World->Tick(FrameTime);
	}
	TestFalse("The team menu closes", Player->IsTeamMenuOpen());
	TestTrue("A terrorist", Player->GetPlayerState<AShooterPlayerState>()->GetTeam() == EShooterTeam::T);
	TestEqual(
		"Five players: three CT bots (the odd one against the player)", CountBots(*GameMode, EShooterTeam::CT), 3);
	TestEqual("One T bot with the player", CountBots(*GameMode, EShooterTeam::T), 1);
	TestTrue("The match started", GameMode->IsMatchInProgress());

	Player->ShowPauseMenu(true);
	TestTrue("Paused", World->IsPaused());
	Player->ReturnToMainMenu();
	TestEqual("Quit to main menu", Context.TravelURL, FString(MainMenuMap));
	Engine->TickWorldTravel(Context, 0.0f);
	TestNotNull("The main menu again",
		Engine->GetGameWorld() != nullptr ? Engine->GetGameWorld()->GetAuthGameMode<AShooterGame_Menu>() : nullptr);
	TestFalse("Not paused", Engine->GetGameWorld() != nullptr && Engine->GetGameWorld()->IsPaused());

	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
