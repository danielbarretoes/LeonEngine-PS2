#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Stats/Stats.h"

#if WITH_DEV_AUTOMATION_TESTS

DECLARE_CYCLE_STAT(TEXT("Test Outer"), STAT_TestOuter, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Test Inner"), STAT_TestInner, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Test Other"), STAT_TestOther, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Test Leaf"), STAT_TestLeaf, STATGROUP_Engine);

namespace
{

	/** Collects from an empty tree while it lives, and leaves the stats off and empty (the tests share the tree). */
	struct FScopedStatsRecording
	{
		FScopedStatsRecording()
		{
			FThreadStats::Reset();
			FThreadStats::MasterEnableAdd();
			FThreadStats::AdvanceFrame();
		}
		~FScopedStatsRecording()
		{
			FThreadStats::MasterEnableSubtract();
			FThreadStats::Reset();
		}
		FScopedStatsRecording(const FScopedStatsRecording&) = delete;
		FScopedStatsRecording& operator=(const FScopedStatsRecording&) = delete;
	};

	/** The node of Stat under Parent, or INDEX_NONE. */
	int32 FindChild(int32 Parent, TStatId Stat)
	{
		for (int32 Child = FThreadStats::GetNode(Parent).FirstChild; Child != INDEX_NONE;
			Child = FThreadStats::GetNode(Child).NextSibling)
		{
			if (FThreadStats::GetNode(Child).Stat == Stat.GetRawPointer())
			{
				return Child;
			}
		}
		return INDEX_NONE;
	}

	/** About 20 microseconds of FPlatformTime::Cycles, the length of the spins the tests time. */
	uint32 GetSpinCycles()
	{
		return uint32(20.0e-6 / FPlatformTime::GetSecondsPerCycle()) + 1;
	}

	void Spin(uint32 Cycles)
	{
		const uint32 Start = FPlatformTime::Cycles();
		while (uint32(FPlatformTime::Cycles() - Start) < Cycles)
		{
		}
	}

	/** Scopes Fanout stats deep under each other down to Levels: Fanout + Fanout^2 + ... nodes. */
	void EnterTree(const TStatId* Stats, int32 Fanout, int32 Levels)
	{
		if (Levels == 0)
		{
			return;
		}
		for (int32 Index = 0; Index < Fanout; ++Index)
		{
			FScopeCycleCounter Scope(Stats[Index]);
			EnterTree(Stats, Fanout, Levels - 1);
		}
	}

	/** Scopes of Stat nested Levels deep. */
	void EnterNested(TStatId Stat, int32 Levels)
	{
		if (Levels == 0)
		{
			return;
		}
		FScopeCycleCounter Scope(Stat);
		EnterNested(Stat, Levels - 1);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsNestingTest, "System.Core.Stats.Nesting",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsNestingTest::RunTest(const FString& Parameters)
{
	FScopedStatsRecording Recording;
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		{
			SCOPE_CYCLE_COUNTER(STAT_TestInner);
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_TestOther);
		}
	}
	{
		// The same stat elsewhere is another node.
		SCOPE_CYCLE_COUNTER(STAT_TestInner);
	}
	const int32 Outer = FindChild(0, GET_STATID(STAT_TestOuter));
	const int32 TopInner = FindChild(0, GET_STATID(STAT_TestInner));
	if (!TestTrue("Top level nodes", Outer != INDEX_NONE && TopInner != INDEX_NONE))
	{
		return false;
	}
	const int32 Inner = FindChild(Outer, GET_STATID(STAT_TestInner));
	const int32 Other = FindChild(Outer, GET_STATID(STAT_TestOther));
	TestTrue("Children of the outer scope", Inner != INDEX_NONE && Other != INDEX_NONE && Inner != TopInner);
	TestEqual("Nodes: the root and four", FThreadStats::GetNumNodes(), 5);
	TestEqual("The first child is the first entered", int32(FThreadStats::GetNode(Outer).FirstChild), Inner);
	TestEqual("Then its sibling", int32(FThreadStats::GetNode(Inner).NextSibling), Other);
	TestEqual("Top level depth", int32(FThreadStats::GetNode(Outer).Depth), 1);
	TestEqual("Child depth", int32(FThreadStats::GetNode(Inner).Depth), 2);
	TestEqual("Parent", int32(FThreadStats::GetNode(Other).Parent), Outer);
	TestEqual("The root counts the frame", FThreadStats::GetNode(0).Counts.Calls, 1u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsInclusiveTimeTest, "System.Core.Stats.InclusiveTime",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsInclusiveTimeTest::RunTest(const FString& Parameters)
{
	FScopedStatsRecording Recording;
	const uint32 SpinCycles = GetSpinCycles();
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		Spin(SpinCycles);
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			SCOPE_CYCLE_COUNTER(STAT_TestInner);
			Spin(SpinCycles);
		}
	}
	const int32 Outer = FindChild(0, GET_STATID(STAT_TestOuter));
	const int32 Inner = Outer != INDEX_NONE ? FindChild(Outer, GET_STATID(STAT_TestInner)) : INDEX_NONE;
	if (!TestTrue("Nodes", Inner != INDEX_NONE))
	{
		return false;
	}
	const uint64 OuterCycles = FThreadStats::GetNode(Outer).Counts.Cycles;
	const uint64 InnerCycles = FThreadStats::GetNode(Inner).Counts.Cycles;
	TestTrue("A node sums its calls' cycles", InnerCycles >= uint64(SpinCycles) * 2);
	TestTrue("A parent includes its children", OuterCycles >= InnerCycles + SpinCycles);

	// A window counts from its start: a second frame of the same work adds as much again.
	FCycleStatsWindow Window;
	Window.Restart();
	FThreadStats::AdvanceFrame();
	{
		SCOPE_CYCLE_COUNTER(STAT_TestInner);
		Spin(SpinCycles);
	}
	TestEqual("Window frames", Window.GetNumFrames(), int64(1));
	const FCycleStatCounts WindowInner = Window.GetStatCounts(GET_STATID(STAT_TestInner));
	TestEqual("Window calls: the new top level node", WindowInner.Calls, 1u);
	TestTrue("Window cycles", WindowInner.Cycles >= SpinCycles);
	TestTrue("The frame's own cycles", Window.GetNodeCounts(0).Cycles > 0);
	TArray<FString> Lines;
	Window.GetReportLines(Lines);
	TestEqual("Report: the scopes of the window", Lines.Num(), 1);
	TestTrue("Report line",
		Lines.Num() == 1 && Lines[0].StartsWith(TEXT("Test Inner: ")) && Lines[0].Contains(TEXT(" ms, 1.0 calls")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsCallsTest, "System.Core.Stats.Calls",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsCallsTest::RunTest(const FString& Parameters)
{
	FScopedStatsRecording Recording;
	for (int32 Pass = 0; Pass < 5; ++Pass)
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		for (int32 Leaf = 0; Leaf < 3; ++Leaf)
		{
			SCOPE_CYCLE_COUNTER(STAT_TestLeaf);
		}
	}
	const int32 Outer = FindChild(0, GET_STATID(STAT_TestOuter));
	const int32 Leaf = Outer != INDEX_NONE ? FindChild(Outer, GET_STATID(STAT_TestLeaf)) : INDEX_NONE;
	if (!TestTrue("Nodes", Leaf != INDEX_NONE))
	{
		return false;
	}
	TestEqual("Outer calls", FThreadStats::GetNode(Outer).Counts.Calls, 5u);
	TestEqual("Leaf calls", FThreadStats::GetNode(Leaf).Counts.Calls, 15u);
	TestEqual("One node per stat and parent", FThreadStats::GetNumNodes(), 3);
	FCycleStatsWindow Window;
	TestEqual(
		"A window from before the frame counts it all", Window.GetStatCounts(GET_STATID(STAT_TestLeaf)).Calls, 15u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsCapacityTest, "System.Core.Stats.Capacity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsCapacityTest::RunTest(const FString& Parameters)
{
	{
		// Deeper than the stack: the deeper scopes are dropped, the others still count, one warning.
		FScopedStatsRecording Recording;
		EnterNested(GET_STATID(STAT_TestOuter), FThreadStats::MaxDepth + 4);
		EnterNested(GET_STATID(STAT_TestOuter), FThreadStats::MaxDepth + 4);
		TestEqual("A node per kept level", FThreadStats::GetNumNodes(), FThreadStats::MaxDepth + 1);
		TestEqual("The deepest kept scope", int32(FThreadStats::GetNode(FThreadStats::MaxDepth).Depth),
			FThreadStats::MaxDepth);
		TestEqual("Its calls", FThreadStats::GetNode(FThreadStats::MaxDepth).Counts.Calls, 2u);
		TestEqual("Warned once", GetWarnings().Num(), 1);
	}
	{
		// More nodes than the tree holds: 4 + 16 + 64 + 256 paths. The scopes that find no node are dropped with
		// their children; the nodes that exist keep counting, and the scopes stay balanced.
		FScopedStatsRecording Recording;
		const TStatId Stats[] = {GET_STATID(STAT_TestOuter), GET_STATID(STAT_TestInner), GET_STATID(STAT_TestOther),
			GET_STATID(STAT_TestLeaf)};
		EnterTree(Stats, 4, 4);
		TestEqual("The tree is full", FThreadStats::GetNumNodes(), FThreadStats::MaxNodes);
		TestEqual("Warned once more", GetWarnings().Num(), 2);
		{
			SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		}
		TestEqual("An existing node still counts", FThreadStats::GetNode(FindChild(0, Stats[0])).Counts.Calls, 2u);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsCostTest, "System.Core.Stats.Cost",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsCostTest::RunTest(const FString& Parameters)
{
	// What a recorded scope costs on this platform (the PS2's in TestPAL's EE log: Budgets.md, N9), inside an outer
	// scope as the engine's are.
	constexpr int32 NumScopes = 10000;
	FScopedStatsRecording Recording;
	uint32 Cycles = 0;
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		const uint32 Start = FPlatformTime::Cycles();
		for (int32 Index = 0; Index < NumScopes; ++Index)
		{
			SCOPE_CYCLE_COUNTER(STAT_TestLeaf);
		}
		Cycles = FPlatformTime::Cycles() - Start;
	}
	const int32 Outer = FindChild(0, GET_STATID(STAT_TestOuter));
	const int32 Leaf = Outer != INDEX_NONE ? FindChild(Outer, GET_STATID(STAT_TestLeaf)) : INDEX_NONE;
	if (!TestTrue("Nodes", Leaf != INDEX_NONE))
	{
		return false;
	}
	TestEqual("Every scope counted", FThreadStats::GetNode(Leaf).Counts.Calls, uint32(NumScopes));
	const double Nanoseconds = double(Cycles) * FPlatformTime::GetSecondsPerCycle() * 1.0e9 / double(NumScopes);
	UE_LOG(LogTemp, Display, TEXT("Cycle stats budget: a recorded scope costs %.0f ns (%.1f FPlatformTime::Cycles)"),
		Nanoseconds, double(Cycles) / double(NumScopes));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsDisabledTest, "System.Core.Stats.Disabled",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStatsDisabledTest::RunTest(const FString& Parameters)
{
	FThreadStats::Reset();
	if (!TestFalse("Nothing collects outside a test that asks", FThreadStats::IsCollectingData()))
	{
		return false;
	}
	FThreadStats::AdvanceFrame();
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		{
			SCOPE_CYCLE_COUNTER(STAT_TestInner);
		}
	}
	TestEqual("No node", FThreadStats::GetNumNodes(), 1);
	TestEqual("No frame", FThreadStats::GetNode(0).Counts.Calls, 0u);

	// A scope that started before the stats stopped still ends, and the next ones record nothing.
	FThreadStats::MasterEnableAdd();
	{
		SCOPE_CYCLE_COUNTER(STAT_TestOuter);
		FThreadStats::MasterEnableSubtract();
		SCOPE_CYCLE_COUNTER(STAT_TestInner);
	}
	TestEqual("Only the scope that started", FThreadStats::GetNumNodes(), 2);
	TestEqual("It ended", FThreadStats::GetNode(1).Counts.Calls, 1u);
	FThreadStats::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
