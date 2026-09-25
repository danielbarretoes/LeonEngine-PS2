#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineFlagsWorkBeforeInitializeTest, "System.Engine.Flags.WorkBeforeInitialize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEngineFlagsWorkBeforeInitializeTest::RunTest(const FString& Parameters)
{
	// The debug flags work on an engine that was never initialized, and the game instance counts the levels opened.
	UGameEngine& Engine = *NewObject<UGameEngine>();
	TestFalse("Not initialized", Engine.IsInitialized());

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

	UGameInstance& GameInstance = *NewObject<UGameInstance>();
	TestEqual("No levels opened", GameInstance.GetLevelsOpened(), 0);
	GameInstance.NotifyLevelOpened();
	TestEqual("One level opened", GameInstance.GetLevelsOpened(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
