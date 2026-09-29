#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FixedStepClock.h"
#include "Misc/AutomationTest.h"
#include "PrimitiveSceneProxy.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"
#include "TickTaskManager.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A logging actor of Group, spawned with its tick set up before it begins play (which registers it). */
	AEngineTestTickRecorder* SpawnRecorder(
		UWorld& World, const TCHAR* Label, ETickingGroup Group = TG_PrePhysics, float Interval = 0.0f)
	{
		AEngineTestTickRecorder* Actor =
			World.SpawnActorDeferred<AEngineTestTickRecorder>(AEngineTestTickRecorder::StaticClass(), FTransform());
		Actor->Label = Label;
		Actor->PrimaryActorTick.TickGroup = Group;
		Actor->PrimaryActorTick.TickInterval = Interval;
		Actor->FinishSpawning(FTransform());
		return Actor;
	}

	/** A logging component on Owner, registered after the owner began play. */
	UEngineTestTickRecorderComponent* AddRecorderComponent(AActor& Owner, const TCHAR* Label)
	{
		UEngineTestTickRecorderComponent* Component = NewObject<UEngineTestTickRecorderComponent>(&Owner);
		Component->Label = Label;
		Component->RegisterComponent();
		return Component;
	}

	FString JoinLog()
	{
		return FString::Join(GetEngineTestTickLog(), TEXT(","));
	}

	/** Ticks the world Count steps of StepSeconds. */
	void TickSteps(UWorld& World, int32 Count, float StepSeconds = 1.0f / 30.0f)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			World.Tick(StepSeconds);
		}
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickGroupsRunInOrderTest, "System.Engine.Tick.GroupsRunInOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickGroupsRunInOrderTest::RunTest(const FString& Parameters)
{
	// The groups run in their order whatever the spawn order: PrePhysics, DuringPhysics, PostPhysics, PostUpdateWork.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)SpawnRecorder(World, TEXT("PostUpdateWork"), TG_PostUpdateWork);
	(void)SpawnRecorder(World, TEXT("PostPhysics"), TG_PostPhysics);
	(void)SpawnRecorder(World, TEXT("PrePhysics"), TG_PrePhysics);
	(void)SpawnRecorder(World, TEXT("DuringPhysics"), TG_DuringPhysics);
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("Group order", JoinLog(), FString(TEXT("PrePhysics,DuringPhysics,PostPhysics,PostUpdateWork")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickRegistrationOrderTest, "System.Engine.Tick.RegistrationOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickRegistrationOrderTest::RunTest(const FString& Parameters)
{
	// A group ticks its actors in their level's order, each one's components just before it in the order they were
	// registered; a tick turned off and on again goes back to its place, and the order holds step after step.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AEngineTestTickRecorder* A = SpawnRecorder(World, TEXT("A"));
	(void)SpawnRecorder(World, TEXT("B"));
	AEngineTestTickRecorder* C = SpawnRecorder(World, TEXT("C"));
	(void)AddRecorderComponent(*A, TEXT("A1"));
	(void)AddRecorderComponent(*C, TEXT("C1"));
	(void)AddRecorderComponent(*A, TEXT("A2"));
	const FString Expected = TEXT("A1,A2,A,B,C1,C");
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("Level order, components first", JoinLog(), Expected);

	A->SetActorTickEnabled(false);
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("A disabled: its components still tick", JoinLog(), FString(TEXT("A1,A2,B,C1,C")));
	A->SetActorTickEnabled(true);
	for (int32 Step = 0; Step < 5; ++Step)
	{
		GetEngineTestTickLog().Reset();
		World.Tick(1.0f / 30.0f);
		TestEqual("Back in its place, every step", JoinLog(), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickEnableDisableTest, "System.Engine.Tick.EnableDisable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickEnableDisableTest::RunTest(const FString& Parameters)
{
	// Only what ticks is visited: an actor that cannot tick (placed geometry) never registers, a disabled tick leaves
	// its group's list; bStartWithTickEnabled false starts off; a destroyed actor's tick leaves at once.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	FTickTaskManager& Manager = World.GetTickTaskManager();
	for (int32 Index = 0; Index < 20; ++Index)
	{
		(void)World.SpawnActor<AStaticMeshActor>();
	}
	TestEqual("Static actors cost nothing", Manager.GetNumEnabledTickFunctions(TG_PrePhysics), 0);

	AEngineTestTickRecorder* Ticking = SpawnRecorder(World, TEXT("On"));
	AEngineTestTickRecorder* Off =
		World.SpawnActorDeferred<AEngineTestTickRecorder>(AEngineTestTickRecorder::StaticClass(), FTransform());
	Off->Label = TEXT("Off");
	Off->PrimaryActorTick.bStartWithTickEnabled = false;
	Off->FinishSpawning(FTransform());
	TestEqual("One enabled", Manager.GetNumEnabledTickFunctions(TG_PrePhysics), 1);
	TestFalse("Starts off", Off->IsActorTickEnabled());
	TickSteps(World, 3);
	TestEqual("On ticks", Ticking->NumTicks, 3);
	TestEqual("Off does not", Off->NumTicks, 0);

	Off->SetActorTickEnabled(true);
	Ticking->SetActorTickEnabled(false);
	TestEqual("Still one enabled", Manager.GetNumEnabledTickFunctions(TG_PrePhysics), 1);
	TickSteps(World, 2);
	TestEqual("On stopped", Ticking->NumTicks, 3);
	TestEqual("Off started", Off->NumTicks, 2);

	Off->Destroy();
	TestEqual("A destroyed actor leaves the list", Manager.GetNumEnabledTickFunctions(TG_PrePhysics), 0);
	TickSteps(World, 2);
	TestEqual("Not ticked after its destruction", Off->NumTicks, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickIntervalTest, "System.Engine.Tick.Interval",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickIntervalTest::RunTest(const FString& Parameters)
{
	// A tick interval skips steps and keeps its rate on average: the remainder carries over, the delta is the time
	// since the last tick. 0.1 s at 30 Hz is every third step; at 25 Hz (0.04 s) it alternates 2 and 3 steps.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AEngineTestTickRecorder* Tenth = SpawnRecorder(World, TEXT("Tenth"), TG_PrePhysics, 0.1f);
	AEngineTestTickRecorder* Every = SpawnRecorder(World, TEXT("Every"));
	TickSteps(World, 2);
	TestEqual("Not before the interval", Tenth->NumTicks, 0);
	TickSteps(World, 1);
	TestEqual("At the third step", Tenth->NumTicks, 1);
	TestEqual("Its delta is the interval", Tenth->LastDeltaSeconds, 0.1f, 1.0e-5f);
	TickSteps(World, 27);
	TestEqual("Ten a second at 30 Hz", Tenth->NumTicks, 10);
	TestEqual("The other every step", Every->NumTicks, 30);

	AEngineTestTickRecorder* Pal = SpawnRecorder(World, TEXT("Pal"), TG_PrePhysics, 0.1f);
	TickSteps(World, 75, 0.04f);
	TestEqual("Ten a second at 25 Hz, over 3 s", Pal->NumTicks, 30);

	// A new interval counts what was waited already.
	Pal->SetActorTickInterval(0.2f);
	TestEqual("Interval changed", Pal->GetActorTickInterval(), 0.2f);
	const int32 Before = Pal->NumTicks;
	TickSteps(World, 25, 0.04f);
	TestEqual("Five a second at 0.2 s", Pal->NumTicks - Before, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickPrerequisitesTest, "System.Engine.Tick.Prerequisites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickPrerequisitesTest::RunTest(const FString& Parameters)
{
	// A tick waits for its prerequisites of the same group and runs right after them (a pawn after its controller:
	// AController::AddPawnTickDependency), its components with it; a prerequisite that goes away no longer holds it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AEngineTestTickRecorder* Pawn = SpawnRecorder(World, TEXT("Pawn"));
	(void)AddRecorderComponent(*Pawn, TEXT("PawnMove"));
	(void)SpawnRecorder(World, TEXT("Other"));
	AEngineTestTickRecorder* Controller = SpawnRecorder(World, TEXT("Controller"));
	Pawn->AddTickPrerequisiteActor(Controller);
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("After its controller", JoinLog(), FString(TEXT("Other,Controller,PawnMove,Pawn")));

	Controller->SetActorTickEnabled(false);
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("A disabled prerequisite does not hold it", JoinLog(), FString(TEXT("PawnMove,Pawn,Other")));
	Controller->SetActorTickEnabled(true);
	Pawn->RemoveTickPrerequisiteActor(Controller);
	GetEngineTestTickLog().Reset();
	World.Tick(1.0f / 30.0f);
	TestEqual("Removed", JoinLog(), FString(TEXT("PawnMove,Pawn,Other,Controller")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickSpawnDuringTickTest, "System.Engine.Tick.SpawnDuringTick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickSpawnDuringTickTest::RunTest(const FString& Parameters)
{
	// An actor spawned while a group runs begins play when the group ends and ticks from the next step.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AEngineTestTickSpawner* Spawner = World.SpawnActor<AEngineTestTickSpawner>();
	Spawner->Victim = World.SpawnActor<AActor>();
	World.Tick(1.0f / 30.0f);
	if (!TestNotNull("Spawned", Spawner->Spawned))
	{
		return false;
	}
	TestFalse("Not in the level during the tick", Spawner->bSpawnedInLevelDuringTick);
	TestTrue("In the level after", World.PersistentLevel->Actors.Contains(Spawner->Spawned));
	TestTrue("Begun after the group", Spawner->Spawned->HasActorBegunPlay());
	return true;
}

// Timers

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerManagerRatesTest, "System.Engine.Timers.Rates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimerManagerRatesTest::RunTest(const FString& Parameters)
{
	// A one-shot timer fires once after its rate, a looping one every rate (after a first delay when given); the
	// queries follow the clock; a rate of 0 sets nothing.
	FTimerManager Timers;
	int32 Once = 0;
	int32 Loops = 0;
	FTimerHandle OnceHandle;
	FTimerHandle LoopHandle;
	Timers.SetTimer(OnceHandle, FTimerDelegate::CreateLambda([&Once]() { ++Once; }), 1.0f, false);
	Timers.SetTimer(LoopHandle, FTimerDelegate::CreateLambda([&Loops]() { ++Loops; }), 0.5f, true, 0.0f);
	TestTrue("Active", Timers.IsTimerActive(OnceHandle));
	TestEqual("Remaining", Timers.GetTimerRemaining(OnceHandle), 1.0f, 1.0e-6f);
	TestEqual("Rate", Timers.GetTimerRate(LoopHandle), 0.5f, 1.0e-6f);
	for (int32 Step = 1; Step <= 29; ++Step)
	{
		Timers.Tick(1.0f / 30.0f);
	}
	TestEqual("Not yet at step 29", Once, 0);
	TestEqual("Elapsed", Timers.GetTimerElapsed(OnceHandle), 29.0f / 30.0f, 1.0e-5f);
	Timers.Tick(1.0f / 30.0f);
	TestEqual("At step 30: exactly 1 s", Once, 1);
	TestFalse("Done", Timers.IsTimerActive(OnceHandle));
	TestEqual("No time for a finished timer", Timers.GetTimerRemaining(OnceHandle), -1.0f);
	TestEqual("First at 0, then every 0.5 s", Loops, 3);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Timers.Tick(1.0f / 30.0f);
	}
	TestEqual("Every 0.5 s", Loops, 7);

	FTimerHandle Zero;
	Timers.SetTimer(Zero, FTimerDelegate::CreateLambda([]() {}), 0.0f, false);
	TestFalse("A rate of 0 sets nothing", Zero.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerManagerClearTest, "System.Engine.Timers.Clear",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimerManagerClearTest::RunTest(const FString& Parameters)
{
	// A callback may clear its own looping timer (it does not come back), clear another one due in the same Tick (it
	// does not fire), and set a new timer on its own handle; a stale handle names nothing.
	FTimerManager Timers;
	FTimerHandle SelfHandle;
	int32 SelfCalls = 0;
	Timers.SetTimer(SelfHandle,
		FTimerDelegate::CreateLambda(
			[&]()
			{
				++SelfCalls;
				TestTrue("A looping timer is active in its callback", Timers.IsTimerActive(SelfHandle));
				if (SelfCalls == 3)
				{
					Timers.ClearTimer(SelfHandle);
				}
			}),
		0.1f, true);
	FTimerHandle Victim;
	int32 VictimCalls = 0;
	FTimerHandle Killer;
	Timers.SetTimer(Killer, FTimerDelegate::CreateLambda([&]() { Timers.ClearTimer(Victim); }), 1.0f, false);
	Timers.SetTimer(Victim, FTimerDelegate::CreateLambda([&VictimCalls]() { ++VictimCalls; }), 1.0f, false);
	FTimerHandle Again;
	int32 AgainCalls = 0;
	Timers.SetTimer(Again,
		FTimerDelegate::CreateLambda(
			[&]()
			{
				++AgainCalls;
				TestFalse("A one-shot timer is done in its callback", Timers.IsTimerActive(Again));
				if (AgainCalls == 1)
				{
					Timers.SetTimer(
						Again, FTimerDelegate::CreateLambda([&AgainCalls]() { ++AgainCalls; }), 0.5f, false);
				}
			}),
		0.5f, false);
	const FTimerHandle Stale = Again;
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Timers.Tick(1.0f / 30.0f);
	}
	TestEqual("Cleared in its third call", SelfCalls, 3);
	TestEqual("Cleared before its turn in the same Tick", VictimCalls, 0);
	TestEqual("Set again from its callback", AgainCalls, 2);
	TestFalse("A stale handle names nothing", Timers.IsTimerActive(Stale));
	TestEqual("Nothing left", Timers.GetNumActiveTimers(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerManagerOrderTest, "System.Engine.Timers.Order",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimerManagerOrderTest::RunTest(const FString& Parameters)
{
	// Due timers fire by their time, those due together in the order they were set; a looping timer that fell behind
	// fires once per period it missed.
	FTimerManager Timers;
	TArray<int32> Order;
	FTimerHandle Handles[4];
	Timers.SetTimer(Handles[0], FTimerDelegate::CreateLambda([&Order]() { Order.Add(0); }), 0.3f, false);
	Timers.SetTimer(Handles[1], FTimerDelegate::CreateLambda([&Order]() { Order.Add(1); }), 0.2f, false);
	Timers.SetTimer(Handles[2], FTimerDelegate::CreateLambda([&Order]() { Order.Add(2); }), 0.3f, false);
	Timers.SetTimer(Handles[3], FTimerDelegate::CreateLambda([&Order]() { Order.Add(3); }), 0.2f, false);
	Timers.Tick(1.0f);
	TestTrue("By time, then set order", Order == TArray<int32>({1, 3, 0, 2}));

	int32 Loops = 0;
	FTimerHandle Loop;
	Timers.SetTimer(Loop, FTimerDelegate::CreateLambda([&Loops]() { ++Loops; }), 0.1f, true);
	Timers.Tick(0.35f);
	TestEqual("Three periods in one Tick", Loops, 3);
	TestEqual("The phase kept", Timers.GetTimerRemaining(Loop), 0.05f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerManagerDeterminismTest, "System.Engine.Timers.Determinism",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimerManagerDeterminismTest::RunTest(const FString& Parameters)
{
	// The clock is integer: over 20 minutes of 1/30 s steps a 0.1 s timer fires exactly 12 000 times and a 40 s one
	// (the bomb's) at exactly step 1 200; over 20 minutes of 1/25 s steps a 0.2 s timer fires exactly 6 000 times. A
	// float sum of the steps would drift.
	FTimerManager Timers;
	int32 Tenths = 0;
	int32 BombStep = 0;
	int32 Step = 0;
	FTimerHandle Tenth;
	FTimerHandle Bomb;
	Timers.SetTimer(Tenth, FTimerDelegate::CreateLambda([&Tenths]() { ++Tenths; }), 0.1f, true);
	Timers.SetTimer(Bomb, FTimerDelegate::CreateLambda([&]() { BombStep = Step; }), 40.0f, false);
	for (Step = 1; Step <= 36000; ++Step)
	{
		Timers.Tick(1.0f / 30.0f);
	}
	TestEqual("12 000 tenths", Tenths, 12000);
	TestEqual("The bomb at step 1 200", BombStep, 1200);
	TestEqual("The clock", Timers.GetTimeUnits(), 1200ull * FTimerManager::TimeUnitsPerSecond);

	FTimerManager Pal;
	int32 Fifths = 0;
	FTimerHandle Fifth;
	Pal.SetTimer(Fifth, FTimerDelegate::CreateLambda([&Fifths]() { ++Fifths; }), 0.2f, true);
	for (int32 PalStep = 0; PalStep < 30000; ++PalStep)
	{
		Pal.Tick(1.0f / 25.0f);
	}
	TestEqual("6 000 fifths at 25 Hz", Fifths, 6000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerManagerWorldTest, "System.Engine.Timers.World",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimerManagerWorldTest::RunTest(const FString& Parameters)
{
	// The world ticks its timers at the start of each step, with its time: a timer an actor sets is due its delay
	// after that step; an actor's timers go when it ends play; the world's time is the timers' clock.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AEngineTestTickRecorder* Actor = SpawnRecorder(World, TEXT("Actor"));
	int32 Calls = 0;
	FTimerHandle Handle;
	World.GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(Actor, &AEngineTestTickRecorder::SetActorTickEnabled, false), 0.1f, false);
	FTimerHandle Counter;
	World.GetTimerManager().SetTimer(
		Counter, FTimerDelegate::CreateLambda([&Calls]() { ++Calls; }), 1.0f / 30.0f, true);
	TickSteps(World, 3);
	TestFalse("Its timer turned its tick off at the third step", Actor->IsActorTickEnabled());
	TestEqual("It ticked twice before, not on that step (the timers come first)", Actor->NumTicks, 2);
	TestEqual("A timer each step", Calls, 3);
	TestEqual("The world's time", World.GetTimeSeconds(), 0.1f, 1.0e-6f);

	FTimerHandle ActorTimer;
	World.GetTimerManager().SetTimer(ActorTimer, Actor, &AActor::LifeSpanExpired, 5.0f);
	Actor->Destroy();
	TestFalse("An actor's timers go with it", World.GetTimerManager().IsTimerActive(ActorTimer));
	return true;
}

// The fixed step (ps2-shipping D4)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedStepClockTest, "System.Engine.FixedStep.Accumulator",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFixedStepClockTest::RunTest(const FString& Parameters)
{
	// Integer microseconds into whole 1/30 s steps: the remainder carries over exactly, the alpha is the remainder's
	// fraction of a step, and a long frame runs at most MaxStepsPerFrame steps (the rest dropped and counted).
	FFixedStepClock Clock(30, 4);
	TestEqual("Step", Clock.GetStepSeconds(), 1.0f / 30.0f);
	TestEqual("33 333 us is not yet a step", Clock.Advance(33333), 0);
	TestEqual("Almost one", Clock.GetAlpha(), 0.99999f, 1.0e-5f);
	TestEqual("One more microsecond is", Clock.Advance(1), 1);
	TestEqual("The remainder: 20 units", Clock.GetAlpha(), 20.0f / 1000000.0f, 1.0e-7f);
	TestEqual("Microseconds to the next step", Clock.GetMicrosecondsToNextStep(), 33333ull);

	FFixedStepClock Exact(30, 4);
	int32 Steps = 0;
	for (int32 Frame = 0; Frame < 3000; ++Frame)
	{
		Steps += Exact.Advance(1000000 / 60);
	}
	// 3000 frames of 16 666 us (60 fps) are 49.998 s: 1 499 steps and 0.94 of the next.
	TestEqual("No drift at 60 fps", Steps, 1499);
	TestEqual("The rest", Exact.GetAlpha(), 0.94f, 1.0e-5f);

	FFixedStepClock Pal(30, 4);
	TArray<int32> PalSteps;
	TArray<float> PalAlphas;
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		PalSteps.Add(Pal.Advance(40000));
		PalAlphas.Add(Pal.GetAlpha());
	}
	TestTrue("PAL (25 fps): 1, 1, 1, 1, 2 steps", PalSteps == TArray<int32>({1, 1, 1, 1, 2}));
	TestEqual("Alpha 0.2", PalAlphas[0], 0.2f, 1.0e-6f);
	TestEqual("Alpha 0.8", PalAlphas[3], 0.8f, 1.0e-6f);
	TestEqual("Alpha 0 after the fifth", PalAlphas[4], 0.0f, 1.0e-6f);

	FFixedStepClock Guard(30, 4);
	TestEqual("A 1 s stall runs at most 4 steps", Guard.Advance(1000000), 4);
	TestEqual("The others are dropped", Guard.GetNumDroppedSteps(), 26ull);
	TestEqual("And their remainder kept", Guard.GetAlpha(), 0.0f, 1.0e-6f);
	Guard.Reset();
	TestEqual("Reset", Guard.GetAlpha(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedStepInterpolationTest, "System.Engine.FixedStep.Interpolation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFixedStepInterpolationTest::RunTest(const FString& Parameters)
{
	// Each step sends the proxies their transform; the render draws a moving proxy between the last two steps, a jump
	// at once, and the interpolation never reaches the component (the simulation).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	if (!TestNotNull("A scene", World.Scene))
	{
		return false;
	}
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>();
	UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	(void)Mesh->SetStaticMesh(Cube);
	if (!TestNotNull("A proxy", Mesh->SceneProxy))
	{
		return false;
	}
	World.Tick(1.0f / 30.0f);
	(void)Actor->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	World.Tick(1.0f / 30.0f);
	World.Scene->InterpolateTransforms(0.25f);
	TestEqual("A quarter of the way", Mesh->SceneProxy->GetLocalToWorld().GetOrigin().X, 25.0f, 1.0e-3f);
	TestEqual("The component is where the step left it", Actor->GetActorLocation().X, 100.0f);
	World.Scene->InterpolateTransforms(1.0f);
	TestEqual("The last step", Mesh->SceneProxy->GetLocalToWorld().GetOrigin().X, 100.0f, 1.0e-3f);

	(void)Actor->SetActorLocation(FVector(5000.0f, 0.0f, 0.0f));
	World.Tick(1.0f / 30.0f);
	World.Scene->InterpolateTransforms(0.25f);
	TestEqual("A jump is drawn at once", Mesh->SceneProxy->GetLocalToWorld().GetOrigin().X, 5000.0f, 1.0e-3f);
	World.Tick(1.0f / 30.0f);
	World.Scene->InterpolateTransforms(0.5f);
	TestEqual("Still: no motion", Mesh->SceneProxy->GetLocalToWorld().GetOrigin().X, 5000.0f, 1.0e-3f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
