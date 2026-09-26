#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineFlagsWorkBeforeInitializeTest, "System.Engine.Flags.WorkBeforeInitialize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineFlagsWorkBeforeInitializeTest::RunTest(const FString& Parameters)
{
	// The debug views are show flags the viewport client's `show` command toggles (F2 and F3 in LeonGame), before any
	// map or window; the game instance counts the levels opened.
	UGameViewportClient& Viewport = *NewObject<UGameViewportClient>();

	TestFalse("Collision debug off by default", Viewport.EngineShowFlags.Collision);
	TestTrue("show Collision", Viewport.Exec(nullptr, TEXT("show Collision"), *GLog));
	TestTrue("Collision debug toggled on", Viewport.EngineShowFlags.Collision);
	TestTrue("show collision again", Viewport.Exec(nullptr, TEXT("show collision"), *GLog));
	TestFalse("Collision debug toggled off", Viewport.EngineShowFlags.Collision);

	TestFalse("Navigation debug off by default", Viewport.EngineShowFlags.Navigation);
	TestTrue("show Navigation", Viewport.Exec(nullptr, TEXT("show Navigation"), *GLog));
	TestTrue("Navigation debug toggled on", Viewport.EngineShowFlags.Navigation);
	TestTrue("show Navigation again", Viewport.Exec(nullptr, TEXT("show Navigation"), *GLog));
	TestFalse("Navigation debug set off", Viewport.EngineShowFlags.Navigation);

	UGameInstance& GameInstance = *NewObject<UGameInstance>();
	TestEqual("No levels opened", GameInstance.GetLevelsOpened(), 0);
	GameInstance.NotifyLevelOpened();
	TestEqual("One level opened", GameInstance.GetLevelsOpened(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
