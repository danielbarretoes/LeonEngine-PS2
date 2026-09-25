#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameViewportClientIgnoreInputTest, "System.Engine.Viewport.IgnoreInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameViewportClientIgnoreInputTest::RunTest(const FString& Parameters)
{
	// An unattended run (a -Screenshot capture) ignores the OS input: the viewport client drops the mouse and key
	// events, so the view stays where the map put it. With the input back, the same mouse sample turns the view.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
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
	UGameViewportClient& Viewport = *Engine->GameViewport;
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (World == nullptr || Controller == nullptr)
	{
		AddError(TEXT("The map did not open with a player"));
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	const FRotator Start = Controller->GetControlRotation();

	Viewport.SetIgnoreInput(true);
	TestTrue("Ignoring", Viewport.IgnoreInput());
	TestFalse("The mouse is dropped", Viewport.InputAxis(nullptr, 0, EKeys::MouseX, 100.0f, 1.0f / 60.0f));
	TestFalse("A key is dropped", Viewport.InputKey(nullptr, 0, EKeys::W, IE_Pressed));
	Viewport.ProcessInput(1.0f / 60.0f);
	World->Tick(1.0f / 60.0f);
	TestTrue("The view stays", Controller->GetControlRotation().Equals(Start, 0.0f));

	Viewport.SetIgnoreInput(false);
	TestTrue("The mouse reaches the player", Viewport.InputAxis(nullptr, 0, EKeys::MouseX, 100.0f, 1.0f / 60.0f));
	World->Tick(1.0f / 60.0f);
	TestFalse("The view turns", Controller->GetControlRotation().Equals(Start, 1.0e-3f));

	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
