#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsoleExecChainTest, "System.Engine.Console.ExecChain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FConsoleExecChainTest::RunTest(const FString& Parameters)
{
	// A local player's command goes to the viewport client (show), the engine (stat, open), then the controller's chain
	// (its Exec UFUNCTIONs: FOV). BaseInput.ini's debug keys and the deferred commands (-ExecCmds) take the same way.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
	const FString BlankMap = TEXT("/Engine/Maps/Entry");
	if (!FPackageName::DoesPackageExist(StarterMap))
	{
		AddError(TEXT("Template_Default.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	UEngine* const SavedEngine = GEngine;
	GEngine = Engine.Get();
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	(void)Engine->Browse(Context, FURL(nullptr, *StarterMap, TRAVEL_Absolute), Error);
	UWorld* World = Engine->GetGameWorld();
	ULocalPlayer& Player = *Engine->GameInstance->GetFirstGamePlayer();
	UGameViewportClient& Viewport = *Engine->GameViewport;
	APlayerController& Controller = *Player.PlayerController;
	TestTrue("The player is the viewport's", Player.ViewportClient == &Viewport);

	TestTrue("show Bounds", Player.Exec(World, TEXT("show Bounds"), *GLog));
	TestTrue("Bounds on", Viewport.EngineShowFlags.Bounds);
	TestTrue("stat unit", Player.Exec(World, TEXT("stat unit"), *GLog));
	TestTrue("Stats on", Engine->IsHudStatsVisible());
	TestTrue("stat unit again", Player.Exec(World, TEXT("stat unit"), *GLog));
	TestFalse("Stats off", Engine->IsHudStatsVisible());
	TestTrue("FOV 75 (the controller's Exec function)", Player.Exec(World, TEXT("FOV 75"), *GLog));
	TestEqual("Camera FOV", Controller.PlayerCameraManager->GetFOVAngle(), 75.0f);
	TestFalse("An unknown command", Player.Exec(World, TEXT("NoSuchCommand 1"), *GLog));

	// F1 runs its bound command when pressed, not when released.
	TestEqual("F1 is bound", Controller.PlayerInput->GetBind(EKeys::F1), FString(TEXT("show Bounds")));
	(void)Controller.InputKey(EKeys::F1, IE_Pressed, 1.0f, false);
	TestFalse("F1 pressed: Bounds off", Viewport.EngineShowFlags.Bounds);
	(void)Controller.InputKey(EKeys::F1, IE_Released, 0.0f, false);
	TestFalse("F1 released: unchanged", Viewport.EngineShowFlags.Bounds);

	Engine->DeferredCommands.Add(TEXT("show AxesGizmo"));
	Engine->TickDeferredCommands();
	TestTrue("A deferred command ran", Viewport.EngineShowFlags.AxesGizmo);
	TestEqual("Once", Engine->DeferredCommands.Num(), 0);

	// `open` travels at the next frame, never inside the world tick.
	const TWeakObjectPtr<APlayerController> OldController = &Controller;
	TestTrue("open", Player.Exec(World, *(FString(TEXT("open ")) + BlankMap), *GLog));
	TestEqual("Travel pending", Context.TravelURL, BlankMap);
	TestTrue("Still Starter", Engine->GetGameWorld() == World);
	Engine->TickWorldTravel(Context, 0.0f);
	TestEqual("Blank opened", Context.LastURL.Map, BlankMap);
	TestFalse("The old controller is gone", OldController.IsValid());
	TestNotNull("A new player controller", Player.PlayerController);

	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
