#include "AI/Navigation/NavigationSystem.h"
#include "AIController.h"
#include "AudioDevice.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/ScopedTestWorld.h"

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
			InBoard.SetValueAsBool(TEXT("DidAct"), true);
			return EBTNodeResult::Succeeded;
		});
	UBTComposite_Sequence Seq(TArray<UBTNode*>{&HasTarget, &Act});

	TestTrue("Sequence fails without target", Seq.Tick(Board, 0.016f) == EBTNodeResult::Failed);
	Board.SetValueAsBool(TEXT("HasTarget"), true);
	TestTrue("Sequence succeeds with target", Seq.Tick(Board, 0.016f) == EBTNodeResult::Succeeded);
	TestEqual("Action ran once", Ran, 1);
	TestTrue("Action wrote the blackboard", Board.GetValueAsBool(TEXT("DidAct")));

	UBTDecorator_Bool Never("Never", true);
	UBTComposite_Selector Sel(TArray<UBTNode*>{&Never, &Act});
	TestTrue("Selector succeeds through the action", Sel.Tick(Board, 0.0f) == EBTNodeResult::Succeeded);
	TestEqual("Action ran twice", Ran, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningAIControllerMoveStatusTest,
	"System.AIModule.FrameworkHardening.AIControllerMoveStatus",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningAIControllerMoveStatusTest::RunTest(const FString& Parameters)
{
	// UE's move status: Moving after MoveToLocation or MoveToActor, Idle after StopMovement.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	AAIController& Ai = *World.SpawnActor<AAIController>();
	Ai.Possess(Character);
	TestTrue("Starts idle", Ai.GetMoveStatus() == EPathFollowingStatus::Idle);
	Ai.MoveToLocation(FVector(300.0f, 0.0f, 0.0f));
	TestTrue("Moving after MoveToLocation", Ai.GetMoveStatus() == EPathFollowingStatus::Moving);
	Ai.StopMovement();
	Ai.MoveToActor(Character);
	TestTrue("Moving after MoveToActor", Ai.GetMoveStatus() == EPathFollowingStatus::Moving);
	Ai.StopMovement();
	TestTrue("Idle after StopMovement", Ai.GetMoveStatus() == EPathFollowingStatus::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningAudioDeviceSilentModeTest,
	"System.AIModule.FrameworkHardening.AudioDeviceSilentModeIsSafeForPlayAPIs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningAudioDeviceSilentModeTest::RunTest(const FString& Parameters)
{
	// A silent audio device makes no buffer and accepts every Play call (even for no buffer) without failing.
	FAudioDevice Audio;
	if (!TestTrue("Silent initialize", Audio.Initialize(/*bInSilent=*/true)))
	{
		return false;
	}
	uint8 Block[FSpuAdpcm::BytesPerBlock] = {};
	Block[1] = FSpuAdpcm::FlagLoopEnd;
	FSpuAdpcmSound Sound;
	Sound.Blocks = Block;
	Sound.NumBlocks = 1;
	Sound.SampleRate = 22050;
	const int32 Buffer = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Silent")), Sound);
	TestEqual("No buffer when silent", Buffer, int32(INDEX_NONE));
	Audio.PlaySound2D(Buffer);
	Audio.PlaySound2D(1234);
	Audio.PlaySoundAtLocation(Buffer, FVector(100.0f, 0.0f, 0.0f));
	Audio.SetUiSound(EUISound::Click, Buffer);
	TestFalse("A silent cue", Audio.HasUiSound(EUISound::Click));
	Audio.PlayUiSound(EUISound::Click);
	Audio.PlayMusic(Buffer);
	TestFalse("No music", Audio.IsMusicPlaying());
	Audio.StopMusic();
	Audio.ReleaseSoundBuffer(Buffer);
	Audio.Tick();
	TestEqual("No voice", Audio.GetNumPlayingVoices(), 0);
	Audio.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameworkHardeningNavAgentRadiusDilationTest,
	"System.AIModule.FrameworkHardening.NavigationAgentRadiusKeepsWideAgentsOutOfGaps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameworkHardeningNavAgentRadiusDilationTest::RunTest(const FString& Parameters)
{
	// Two pillars 1 m apart: an agent 40 cm wide walks between them, one 60 cm wide does not.
	FPhysScene Physics;
	FBodyInstance Floor{};
	Floor.Type = EBodyType::Static;
	Floor.Position = FVector(0.0f, 0.0f, -50.0f);
	Floor.HalfExtents = FVector(2000.0f, 2000.0f, 50.0f);
	Physics.GetBodies().Add(Floor);
	for (const float Y : {-120.0f, 120.0f})
	{
		FBodyInstance Pillar{};
		Pillar.Type = EBodyType::Static;
		Pillar.Position = FVector(0.0f, Y, 150.0f);
		Pillar.HalfExtents = FVector(40.0f, 70.0f, 150.0f);
		Physics.GetBodies().Add(Pillar);
	}
	FWaypointLinkParams Narrow;
	Narrow.AgentRadius = 40.0f;
	FWaypointLinkParams Wide = Narrow;
	Wide.AgentRadius = 60.0f;
	const FVector From(-300.0f, 0.0f, 50.0f);
	const FVector To(300.0f, 0.0f, 50.0f);
	TestTrue("A narrow agent passes", UNavigationSystem::CanWalkBetween(Physics, From, To, Narrow));
	TestFalse("A wide agent does not", UNavigationSystem::CanWalkBetween(Physics, From, To, Wide));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
