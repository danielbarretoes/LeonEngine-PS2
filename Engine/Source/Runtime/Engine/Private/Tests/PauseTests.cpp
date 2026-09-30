#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// UE's pause (ps2-polish P9): a paused world's time, timers and actors stand still, the players' input and HUDs go on;
// UGameplayStatics::OpenLevel travels with URL options; SetInputMode frees or captures the mouse.

namespace
{
	constexpr float StepSeconds = 1.0f / 30.0f;

	/** A logging actor that ticks while the world is paused when bWhenPaused. */
	AEngineTestTickRecorder* SpawnRecorder(UWorld& World, const TCHAR* Label, bool bWhenPaused)
	{
		AEngineTestTickRecorder* Actor =
			World.SpawnActorDeferred<AEngineTestTickRecorder>(AEngineTestTickRecorder::StaticClass(), FTransform());
		Actor->Label = Label;
		Actor->PrimaryActorTick.bTickEvenWhenPaused = bWhenPaused;
		Actor->FinishSpawning(FTransform());
		return Actor;
	}

	void TickSteps(UWorld& World, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			World.Tick(StepSeconds);
		}
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPauseTest, "System.Engine.World.Pause",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWorldPauseTest::RunTest(const FString& Parameters)
{
	// A player pauses (UGameplayStatics::SetGamePaused, the first controller's SetPause, the game mode's pauser): the
	// world's time and timers stop and only what ticks when paused ticks (the player controllers, the HUDs); the real
	// time goes on. Going on again resumes the time where it stopped, and the timer fires as late as the pause lasted.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AWorldSettings* Settings = World.SpawnActor<AWorldSettings>();
	World.PersistentLevel->SetWorldSettings(Settings);
	(void)World.SetGameMode(AGameModeBase::StaticClass());
	APlayerController* Player = World.SpawnActor<APlayerController>();
	const AHUD* HUD = World.SpawnActor<AHUD>();
	if (!TestNotNull("A player", Player) || !TestNotNull("with a state", Player->GetPlayerState<APlayerState>()) ||
		!TestNotNull("A HUD", HUD))
	{
		return false;
	}
	TestTrue("The player's input ticks when paused", Player->PrimaryActorTick.bTickEvenWhenPaused != 0);
	TestTrue("and the HUD's widgets", HUD->PrimaryActorTick.bTickEvenWhenPaused != 0);
	(void)SpawnRecorder(World, TEXT("Game"), false);
	(void)SpawnRecorder(World, TEXT("UI"), true);
	int32 Fired = 0;
	FTimerHandle Timer;
	World.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([&Fired]() { ++Fired; }), 0.5f, false);

	GetEngineTestTickLog().Reset();
	TickSteps(World, 3);
	TestEqual("Playing: both tick", FString::Join(GetEngineTestTickLog(), TEXT(",")),
		FString(TEXT("Game,UI,Game,UI,Game,UI")));
	TestFalse("Not paused", UGameplayStatics::IsGamePaused(&World));

	TestTrue("Paused", UGameplayStatics::SetGamePaused(&World, true));
	TestTrue("IsGamePaused", UGameplayStatics::IsGamePaused(&World) && World.IsPaused() && Player->IsPaused());
	TestTrue("By this player", Settings->GetPauserPlayerState() == Player->GetPlayerState<APlayerState>());
	TestTrue("who may go on", Player->CanUnpause());
	TestFalse("Pausing again changes nothing", Player->SetPause(true));
	const float Time = World.GetTimeSeconds();
	const float RealTime = World.GetRealTimeSeconds();
	GetEngineTestTickLog().Reset();
	TickSteps(World, 30);
	TArray<FString> OnlyUI;
	OnlyUI.Init(TEXT("UI"), 30);
	TestEqual("Paused: only the UI ticks", FString::Join(GetEngineTestTickLog(), TEXT(",")),
		FString::Join(OnlyUI, TEXT(",")));
	TestEqual("The world's time stands still", World.GetTimeSeconds(), Time);
	TestEqual("The real time goes on", World.GetRealTimeSeconds(), RealTime + (30.0f * StepSeconds), 1.0e-3f);
	TestEqual("The timer waits", Fired, 0);
	TestTrue("and keeps its time", World.GetTimerManager().IsTimerActive(Timer));

	TestTrue("Going on", UGameplayStatics::SetGamePaused(&World, false));
	TestFalse("Not paused any more", World.IsPaused() || Settings->GetPauserPlayerState() != nullptr);
	TestFalse("Going on again changes nothing", Player->SetPause(false));
	TickSteps(World, 11);
	TestEqual("The timer's half second: 3 steps before and 12 after, not the pause's", Fired, 0);
	TickSteps(World, 1);
	TestEqual("then it fires", Fired, 1);
	TestEqual("The time went on from where it stopped", World.GetTimeSeconds(), Time + (12.0f * StepSeconds), 1.0e-3f);

	// The Pause command toggles it.
	Player->Pause();
	TestTrue("Pause", World.IsPaused());
	Player->Pause();
	TestFalse("Pause again", World.IsPaused());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTravelOpenLevelTest, "System.Engine.Travel.OpenLevel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTravelOpenLevelTest::RunTest(const FString& Parameters)
{
	// OpenLevel queues the travel for the next frame with its options (UEngine::SetClientTravel), absolute by default;
	// the new world's game mode gets them (OptionsString). SetInputMode sets how the viewport treats the mouse.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
	const FString BlankMap = TEXT("/Engine/Maps/Entry");
	if (!FPackageName::DoesPackageExist(StarterMap) || !FPackageName::DoesPackageExist(BlankMap))
	{
		AddError(TEXT("Template_Default.lmap or Entry.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	UEngine* const SavedEngine = GEngine;
	GEngine = Engine.Get();
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	(void)Engine->Browse(Context, FURL(nullptr, *(StarterMap + TEXT("?Old=1")), TRAVEL_Absolute), Error);
	UWorld* World = Engine->GetGameWorld();
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (!TestNotNull("A world", World) || !TestNotNull("A player", Controller))
	{
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	TestTrue("GetPlayerController", UGameplayStatics::GetPlayerController(World, 0) == Controller);
	TestNull("Only one", UGameplayStatics::GetPlayerController(World, 1));

	Controller->SetInputMode(FInputModeUIOnly());
	TestTrue(
		"UI only: the cursor is free", Engine->GameViewport->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
	Controller->SetInputMode(FInputModeGameAndUI());
	TestTrue("Game and UI: a press looks",
		Engine->GameViewport->GetMouseCaptureMode() == EMouseCaptureMode::CaptureDuringMouseDown);
	Controller->SetInputMode(FInputModeGameOnly());
	TestTrue("Game only: captured",
		Engine->GameViewport->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);

	UGameplayStatics::OpenLevel(Controller, FName(*BlankMap), true, TEXT("bots=4?difficulty=Hard"));
	TestEqual("The travel waits for the next frame", Context.TravelURL, BlankMap + TEXT("?bots=4?difficulty=Hard"));
	TestTrue("in the same world", Engine->GetGameWorld() == World);
	Engine->TickWorldTravel(Context, 0.0f);
	TestEqual("The map", Context.LastURL.Map, BlankMap);
	TestTrue("Its options",
		Context.LastURL.Op.Contains(TEXT("bots=4")) && Context.LastURL.Op.Contains(TEXT("difficulty=Hard")));
	TestFalse("Absolute: the old options are gone", Context.LastURL.Op.Contains(TEXT("Old=1")));
	const AGameModeBase* GameMode =
		Engine->GetGameWorld() != nullptr ? Engine->GetGameWorld()->GetAuthGameMode() : nullptr;
	TestTrue("The game mode has them",
		GameMode != nullptr && UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("bots"), 0) == 4 &&
			UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("difficulty")) == TEXT("Hard"));

	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
