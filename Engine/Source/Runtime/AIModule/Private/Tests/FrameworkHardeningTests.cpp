#include "AI/Navigation/NavigationSystem.h"
#include "AudioDevice.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "GameFramework/HUD.h"
#include "GameplayMinimal.h"
#include "Level/LeonLevelFormat.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningBehaviorTreeSequenceAndSelectorTest,
	"System.AIModule.FrameworkHardening.BehaviorTreeSequenceAndSelectorWithBlackboard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningBehaviorTreeSequenceAndSelectorTest::RunTest(const FString& Parameters)
{
	// A Sequence runs its action only once the blackboard gate is open; a Selector falls through a failing child.
	UBlackboardComponent Board;
	UBTDecorator_Bool HasTarget("HasTarget", true);
	int32 Ran = 0;
	UBTTask_Action Act(
		[&Ran](UBlackboardComponent& InBoard, float)
		{
			++Ran;
			InBoard.SetBool("DidAct", true);
			return EBTNodeResult::Succeeded;
		});
	UBTComposite_Sequence Seq(TArray<UBTNode*>{&HasTarget, &Act});

	TestTrue("Sequence fails without target", Seq.Tick(Board, 0.016f) == EBTNodeResult::Failed);
	Board.SetBool("HasTarget", true);
	TestTrue("Sequence succeeds with target", Seq.Tick(Board, 0.016f) == EBTNodeResult::Succeeded);
	TestEqual("Action ran once", Ran, 1);
	TestTrue("Action wrote the blackboard", Board.GetBool("DidAct"));

	UBTDecorator_Bool Never("Never", true);
	UBTComposite_Selector Sel(TArray<UBTNode*>{&Never, &Act});
	TestTrue("Selector succeeds through the action", Sel.Tick(Board, 0.0f) == EBTNodeResult::Succeeded);
	TestEqual("Action ran twice", Ran, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningAIControllerLogicStateTest,
	"System.AIModule.FrameworkHardening.AIControllerLogicStateTracksMoveToChaseIdle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningAIControllerLogicStateTest::RunTest(const FString& Parameters)
{
	// The AI logic state follows MoveToLocation (MoveTo), MoveToActor (Chase) and StopMovement (Idle).
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	AAIController Ai;
	Ai.Possess(Character);
	TestTrue("Starts idle", Ai.GetLogicState() == EAILogicState::Idle);
	Ai.MoveToLocation(FVector(300.0f, 0.0f, 0.0f));
	TestTrue("MoveTo after MoveToLocation", Ai.GetLogicState() == EAILogicState::MoveTo);
	Ai.MoveToActor(Character);
	TestTrue("Chase after MoveToActor", Ai.GetLogicState() == EAILogicState::Chase);
	Ai.StopMovement();
	TestTrue("Idle after StopMovement", Ai.GetLogicState() == EAILogicState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningAudioDeviceSilentModeTest,
	"System.AIModule.FrameworkHardening.AudioDeviceSilentModeIsSafeForPlayAPIs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningAudioDeviceSilentModeTest::RunTest(const FString& Parameters)
{
	// A silent audio device accepts every Play call (even for missing files) without failing.
	FAudioDevice Audio;
	if (!TestTrue("Silent initialize", Audio.Initialize(/*bInSilent=*/true)))
	{
		return false;
	}
	Audio.PlaySound2D("does-not-exist.wav");
	Audio.PlayUiSound(EUISound::Click);
	Audio.PlayMusic("MenuBed.wav");
	Audio.StopMusic();
	Audio.Tick();
	Audio.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningHUDAddWidgetTextBlockAndRemoveTest,
	"System.AIModule.FrameworkHardening.HUDAddWidgetTextBlockAndRemove",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningHUDAddWidgetTextBlockAndRemoveTest::RunTest(const FString& Parameters)
{
	// The HUD finds an added TextBlock by class and forgets it once removed.
	AHUD Hud;
	UTextBlock* Text = Hud.AddWidget<UTextBlock>();
	if (!TestNotNull("Added TextBlock", Text))
	{
		return false;
	}
	Text->SetText(FText::FromString("Hello"));
	TestTrue("Found by class", Hud.GetWidgetOfClass<UTextBlock>() == Text);
	Hud.Tick(0.016f);
	Hud.RemoveWidget(Text);
	TestNull("Gone after remove", Hud.GetWidgetOfClass<UTextBlock>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningDeserializeLeonLevelAdversarialInputsTest,
	"System.AIModule.FrameworkHardening.DeserializeLeonLevelAdversarialInputs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningDeserializeLeonLevelAdversarialInputsTest::RunTest(const FString& Parameters)
{
	// DeserializeLeonLevel rejects empty, junk and header-only buffers (the first two log a bad magic).
	AddExpectedError("bad magic", 2);
	FLevelDocument Doc;
	const TArray<uint8> Empty;
	TestFalse("Empty rejected", DeserializeLeonLevel(Empty, Doc));

	TArray<uint8> Junk;
	Junk.Init(0xA5, 64);
	TestFalse("Junk rejected", DeserializeLeonLevel(Junk, Doc));

	const TArray<uint8> AlmostMagic = {'L', 'L', 'E', 'V', 1, 0, 0, 0};
	TestFalse("Header only rejected", DeserializeLeonLevel(AlmostMagic, Doc));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningNavAgentRadiusDilationTest,
	"System.AIModule.FrameworkHardening.NavigationSystemAgentRadiusDilationShrinksWalkableRing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningNavAgentRadiusDilationTest::RunTest(const FString& Parameters)
{
	// A wider agent radius dilates the pillar more, leaving fewer walkable cells.
	FPhysScene Physics;
	FBodyInstance Floor{};
	Floor.Type = EBodyType::Static;
	Floor.Position = FVector(0.0f, 0.0f, 0.0f);
	Floor.HalfExtents = FVector(2000.0f, 2000.0f, 50.0f);
	Physics.GetBodies().Add(Floor);

	FBodyInstance Pillar{};
	Pillar.Type = EBodyType::Static;
	Pillar.Position = FVector(0.0f, 0.0f, 100.0f);
	Pillar.HalfExtents = FVector(40.0f, 40.0f, 150.0f);
	Physics.GetBodies().Add(Pillar);

	UNavigationSystem Narrow;
	Narrow.SetCellSize(50.0f);
	Narrow.SetAgentRadius(35.0f);
	Narrow.BuildFromPhysScene(Physics, 0.0f, 1000.0f);

	UNavigationSystem Wide;
	Wide.SetCellSize(50.0f);
	Wide.SetAgentRadius(150.0f);
	Wide.BuildFromPhysScene(Physics, 0.0f, 1000.0f);

	TestTrue("Wide agent has fewer walkable cells", Wide.GetWalkableCellCount() < Narrow.GetWalkableCellCount());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
