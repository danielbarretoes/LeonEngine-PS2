#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineFlagsWorkBeforeInitializeTest, "System.Engine.Flags.WorkBeforeInitialize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineFlagsWorkBeforeInitializeTest::RunTest(const FString& Parameters)
{
	// The debug / input flags and the game instance counters work on an engine that was never initialized.
	UGameEngine Engine;
	TestFalse("Not initialized", Engine.IsInitialized());

	Engine.SetSuppressCameraDrag(true);
	TestTrue("Camera drag suppressed", Engine.IsCameraDragSuppressed());
	Engine.SetSuppressCameraDrag(false);
	TestFalse("Camera drag not suppressed", Engine.IsCameraDragSuppressed());

	TestFalse("Collision debug off by default", Engine.IsCollisionDebugEnabled());
	Engine.ToggleCollisionDebug();
	TestTrue("Collision debug toggled on", Engine.IsCollisionDebugEnabled());
	Engine.SetCollisionDebugEnabled(false);
	TestFalse("Collision debug set off", Engine.IsCollisionDebugEnabled());

	TestFalse("NavMesh debug off by default", Engine.IsNavMeshDebugEnabled());
	Engine.ToggleNavMeshDebug();
	TestTrue("NavMesh debug toggled on", Engine.IsNavMeshDebugEnabled());
	Engine.SetNavMeshDebugEnabled(false);
	TestFalse("NavMesh debug set off", Engine.IsNavMeshDebugEnabled());

	Engine.SetKeyboardOrbitEnabled(false);
	Engine.SetOrbitMouseEnabled(false);

	TestEqual("No levels opened", Engine.GetGameInstance().GetLevelsOpened(), 0);
	Engine.GetGameInstance().NotifyLevelOpened();
	TestEqual("One level opened", Engine.GetGameInstance().GetLevelsOpened(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
