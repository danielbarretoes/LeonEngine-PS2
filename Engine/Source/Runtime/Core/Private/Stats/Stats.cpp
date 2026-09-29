#include "Stats/Stats.h"

#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogStats, Log, All);

int32 FThreadStats::MasterEnableCounter = 0;

namespace
{

	/** A scope that is open: its node (INDEX_NONE when dropped) and the counters when it started. */
	struct FOpenScope
	{
		int32 Node = INDEX_NONE;
		uint32 StartCycles = 0;
		uint32 StartEvents[FCycleStatCounts::MaxPerfCounters] = {};
	};

	/** The game thread's record, in static arrays: nothing is allocated while the stats collect. */
	struct FCycleStatsState
	{
		FCycleStatNode Nodes[FThreadStats::MaxNodes];
		int32 NumNodes = 1;
		FOpenScope Stack[FThreadStats::MaxDepth];
		/** The open scopes; past MaxDepth they are counted but not kept (dropped). */
		int32 Depth = 0;
		uint32 ResetSerial = 0;
		uint32 FrameStartCycles = 0;
		bool bHasFrameStart = false;
		bool bWarnedTreeFull = false;
		bool bWarnedTooDeep = false;
	};

	FCycleStatsState GCycleStats;

	/** Stat's node under Parent, added at the end of the parent's children when new; INDEX_NONE when the tree is full.
	 */
	int32 FindOrAddNode(int32 Parent, FCycleStat& Stat)
	{
		FCycleStatsState& State = GCycleStats;
		const int32 Cached = Stat.CachedNode;
		if (Cached > 0 && Cached < State.NumNodes && State.Nodes[Cached].Stat == &Stat &&
			State.Nodes[Cached].Parent == Parent)
		{
			return Cached;
		}
		int32 LastChild = INDEX_NONE;
		for (int32 Child = State.Nodes[Parent].FirstChild; Child != INDEX_NONE; Child = State.Nodes[Child].NextSibling)
		{
			if (State.Nodes[Child].Stat == &Stat)
			{
				Stat.CachedNode = Child;
				return Child;
			}
			LastChild = Child;
		}
		if (State.NumNodes >= FThreadStats::MaxNodes)
		{
			if (!State.bWarnedTreeFull)
			{
				State.bWarnedTreeFull = true;
				UE_LOG(LogStats, Warning, "Cycle stats: the tree is full (%d nodes); new scopes such as %s are dropped",
					FThreadStats::MaxNodes, Stat.StatName);
			}
			return INDEX_NONE;
		}
		const int32 NodeIndex = State.NumNodes++;
		FCycleStatNode& Node = State.Nodes[NodeIndex];
		Node = FCycleStatNode();
		Node.Stat = &Stat;
		Node.Parent = int16(Parent);
		Node.Depth = int16(State.Nodes[Parent].Depth + 1);
		if (LastChild == INDEX_NONE)
		{
			State.Nodes[Parent].FirstChild = int16(NodeIndex);
		}
		else
		{
			State.Nodes[LastChild].NextSibling = int16(NodeIndex);
		}
		Stat.CachedNode = NodeIndex;
		return NodeIndex;
	}

	void AddCounts(FCycleStatCounts& Sum, const FCycleStatCounts& Counts)
	{
		Sum.Cycles += Counts.Cycles;
		Sum.Calls += Counts.Calls;
		for (int32 Index = 0; Index < FCycleStatCounts::MaxPerfCounters; ++Index)
		{
			Sum.Events[Index] += Counts.Events[Index];
		}
	}

} // namespace

void FThreadStats::MasterEnableAdd(int32 Value)
{
	const bool bWasCollecting = IsCollectingData();
	MasterEnableCounter += Value;
	if (!bWasCollecting && IsCollectingData())
	{
		// The first frame's own cycles start at its AdvanceFrame.
		GCycleStats.bHasFrameStart = false;
		FPlatformTime::EnablePerfCounters(true);
	}
}

void FThreadStats::MasterEnableSubtract(int32 Value)
{
	const bool bWasCollecting = IsCollectingData();
	MasterEnableCounter = FMath::Max(0, MasterEnableCounter - Value);
	if (bWasCollecting && !IsCollectingData())
	{
		FPlatformTime::EnablePerfCounters(false);
	}
}

void FThreadStats::AdvanceFrame()
{
	if (!IsCollectingData())
	{
		return;
	}
	FCycleStatsState& State = GCycleStats;
	const uint32 Now = FPlatformTime::Cycles();
	FCycleStatCounts& Frame = State.Nodes[0].Counts;
	if (State.bHasFrameStart)
	{
		Frame.Cycles += uint32(Now - State.FrameStartCycles);
	}
	++Frame.Calls;
	State.FrameStartCycles = Now;
	State.bHasFrameStart = true;
	// The counters restart only between scopes: an open scope's start would be lost.
	if (State.Depth == 0)
	{
		for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
		{
			Frame.Events[Index] += FPlatformTime::ReadPerfCounter(Index);
		}
		FPlatformTime::ResetPerfCounters();
	}
}

void FThreadStats::Enter(FCycleStat& Stat)
{
	FCycleStatsState& State = GCycleStats;
	if (State.Depth >= MaxDepth)
	{
		++State.Depth;
		if (!State.bWarnedTooDeep)
		{
			State.bWarnedTooDeep = true;
			UE_LOG(LogStats, Warning, "Cycle stats: scopes nest deeper than %d; the deeper ones such as %s are dropped",
				MaxDepth, Stat.StatName);
		}
		return;
	}
	FOpenScope& Scope = State.Stack[State.Depth];
	const int32 Parent = State.Depth > 0 ? State.Stack[State.Depth - 1].Node : 0;
	++State.Depth;
	// A dropped scope drops the scopes inside it.
	Scope.Node = Parent != INDEX_NONE ? FindOrAddNode(Parent, Stat) : INDEX_NONE;
	if (Scope.Node == INDEX_NONE)
	{
		return;
	}
	for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
	{
		Scope.StartEvents[Index] = FPlatformTime::ReadPerfCounter(Index);
	}
	Scope.StartCycles = FPlatformTime::Cycles();
}

void FThreadStats::Exit()
{
	const uint32 Now = FPlatformTime::Cycles();
	uint32 Events[FCycleStatCounts::MaxPerfCounters] = {};
	for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
	{
		Events[Index] = FPlatformTime::ReadPerfCounter(Index);
	}
	FCycleStatsState& State = GCycleStats;
	if (State.Depth <= 0)
	{
		return;
	}
	--State.Depth;
	if (State.Depth >= MaxDepth)
	{
		return;
	}
	const FOpenScope& Scope = State.Stack[State.Depth];
	if (Scope.Node == INDEX_NONE)
	{
		return;
	}
	FCycleStatCounts& Counts = State.Nodes[Scope.Node].Counts;
	// Unsigned 32-bit differences: a counter that wrapped inside the scope still gives its length.
	Counts.Cycles += uint32(Now - Scope.StartCycles);
	++Counts.Calls;
	for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
	{
		Counts.Events[Index] += Events[Index] - Scope.StartEvents[Index];
	}
}

int32 FThreadStats::GetNumNodes()
{
	return GCycleStats.NumNodes;
}

const FCycleStatNode& FThreadStats::GetNode(int32 Index)
{
	check(Index >= 0 && Index < GCycleStats.NumNodes);
	return GCycleStats.Nodes[Index];
}

uint32 FThreadStats::GetResetSerial()
{
	return GCycleStats.ResetSerial;
}

void FThreadStats::Reset()
{
	FCycleStatsState& State = GCycleStats;
	State.Nodes[0] = FCycleStatNode();
	State.NumNodes = 1;
	for (int32 Level = 0; Level < FMath::Min(State.Depth, int32(MaxDepth)); ++Level)
	{
		State.Stack[Level].Node = INDEX_NONE;
	}
	++State.ResetSerial;
	State.bHasFrameStart = false;
	State.bWarnedTreeFull = false;
	State.bWarnedTooDeep = false;
}

void FCycleStatsWindow::Restart()
{
	const int32 NumNodes = FThreadStats::GetNumNodes();
	Start.Reset();
	Start.Reserve(NumNodes);
	for (int32 Index = 0; Index < NumNodes; ++Index)
	{
		Start.Add(FThreadStats::GetNode(Index).Counts);
	}
	StartResetSerial = FThreadStats::GetResetSerial();
}

FCycleStatCounts FCycleStatsWindow::GetNodeCounts(int32 NodeIndex) const
{
	FCycleStatCounts Counts = FThreadStats::GetNode(NodeIndex).Counts;
	if (StartResetSerial == FThreadStats::GetResetSerial() && Start.IsValidIndex(NodeIndex))
	{
		const FCycleStatCounts& Before = Start[NodeIndex];
		Counts.Cycles -= Before.Cycles;
		Counts.Calls -= Before.Calls;
		for (int32 Index = 0; Index < FCycleStatCounts::MaxPerfCounters; ++Index)
		{
			Counts.Events[Index] -= Before.Events[Index];
		}
	}
	return Counts;
}

int64 FCycleStatsWindow::GetNumFrames() const
{
	return int64(GetNodeCounts(0).Calls);
}

FCycleStatCounts FCycleStatsWindow::GetStatCounts(TStatId StatId) const
{
	FCycleStatCounts Sum;
	for (int32 Index = 1; Index < FThreadStats::GetNumNodes(); ++Index)
	{
		if (FThreadStats::GetNode(Index).Stat == StatId.GetRawPointer())
		{
			AddCounts(Sum, GetNodeCounts(Index));
		}
	}
	return Sum;
}

double FCycleStatsWindow::GetMillisecondsPerFrame(uint64 Cycles) const
{
	const int64 Frames = GetNumFrames();
	return Frames > 0 ? double(Cycles) * FPlatformTime::GetSecondsPerCycle() * 1000.0 / double(Frames) : 0.0;
}

void FCycleStatsWindow::GetReportLines(
	TArray<FString>& OutLines, double MinMilliseconds, int32 MaxLineDepth, int32 MaxLines) const
{
	OutLines.Reset();
	const double Frames = double(FMath::Max<int64>(1, GetNumFrames()));
	// Depth first from the root's children: a node, then its children, then its next sibling.
	int32 Node = FThreadStats::GetNode(0).FirstChild;
	while (Node != INDEX_NONE && OutLines.Num() < MaxLines)
	{
		const FCycleStatNode& Info = FThreadStats::GetNode(Node);
		const FCycleStatCounts Counts = GetNodeCounts(Node);
		const double Milliseconds = GetMillisecondsPerFrame(Counts.Cycles);
		const bool bShown = Info.Depth <= MaxLineDepth && Counts.Calls > 0 && Milliseconds >= MinMilliseconds;
		if (bShown)
		{
			FString Line = FString::Printf("%*s%s: %.2f ms, %.1f calls", (Info.Depth - 1) * 2, "",
				Info.Stat->Description, Milliseconds, double(Counts.Calls) / Frames);
			if (Info.Depth == 1)
			{
				for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
				{
					Line += FString::Printf(
						", %.0f %s", double(Counts.Events[Index]) / Frames, FPlatformTime::GetPerfCounterName(Index));
				}
			}
			OutLines.Add(MoveTemp(Line));
		}
		// Into the children of a shown node, else on to the next sibling (or an ancestor's).
		int32 Next = bShown ? int32(Info.FirstChild) : INDEX_NONE;
		for (int32 Up = Node; Next == INDEX_NONE && Up > 0; Up = FThreadStats::GetNode(Up).Parent)
		{
			Next = FThreadStats::GetNode(Up).NextSibling;
		}
		Node = Next;
	}
}
