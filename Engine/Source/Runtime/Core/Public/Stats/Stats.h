#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreMiscDefines.h"

/**
 * Cycle stats (UE: Stats2.h and the console's `stat` pages; Docs/PLANS/ps2-shipping.md N9): scoped counters that time a
 * frame's work as a hierarchy, on every platform.
 *
 * - DECLARE_STATS_GROUP names a group; DECLARE_CYCLE_STAT declares a stat in a .cpp (DECLARE_CYCLE_STAT_EXTERN in a
 *   header, with DEFINE_STAT in one .cpp); SCOPE_CYCLE_COUNTER(STAT_X) times the rest of the enclosing block.
 * - A scope records only while the stats collect (FThreadStats::IsCollectingData: `-LogFrameTimes`, `stat cycles`);
 *   otherwise it costs that test.
 * - The record is a tree in static arrays: a node for each stat under each parent it ran in (MaxNodes), and a stack of
 *   the open scopes (MaxDepth); no allocation per scope. A node sums its inclusive FPlatformTime::Cycles, its calls and
 *   the platform's event counters (the EE's cache misses). The root is the frame (FThreadStats::AdvanceFrame). A scope
 *   that does not fit (the tree is full, or it nests deeper than MaxDepth) is dropped with the scopes inside it, and a
 *   warning is logged once.
 * - The game thread's (Leon's engine is single threaded).
 * - FCycleStatsWindow reads the counts over a stretch of frames: `-LogFrameTimes`' log and run summary, `stat cycles`.
 */

/** A cycle stat, what DECLARE_CYCLE_STAT defines (UE: the FStat_<Id> type and the data its TStatId points at). */
struct FCycleStat
{
	constexpr FCycleStat(const TCHAR* InDescription, const TCHAR* InStatName, const TCHAR* InGroupName)
		: Description(InDescription)
		, StatName(InStatName)
		, GroupName(InGroupName)
	{
	}

	/** The name shown ("World Tick"). */
	const TCHAR* Description;
	/** The identifier (`STAT_WorldTick`). */
	const TCHAR* StatName;
	/** The group's identifier (`STATGROUP_Engine`). */
	const TCHAR* GroupName;
	/** The node the stat was last entered as: FThreadStats' cache, checked before use. */
	int32 CachedNode = INDEX_NONE;
};

/** A cycle stat's handle (UE: TStatId); GET_STATID makes one. */
class TStatId
{
public:
	constexpr TStatId() = default;
	constexpr explicit TStatId(FCycleStat* InStat)
		: Stat(InStat)
	{
	}

	[[nodiscard]] bool IsValidStat() const
	{
		return Stat != nullptr;
	}
	[[nodiscard]] FCycleStat* GetRawPointer() const
	{
		return Stat;
	}
	[[nodiscard]] bool operator==(TStatId Other) const
	{
		return Stat == Other.Stat;
	}

private:
	FCycleStat* Stat = nullptr;
};

/** What a node of the tree counted (Leon). */
struct FCycleStatCounts
{
	static constexpr int32 MaxPerfCounters = 2;

	/** Inclusive FPlatformTime::Cycles: the root's are the frames' own, from one AdvanceFrame to the next. */
	uint64 Cycles = 0;
	/** The scopes that ended; the root's are the frames. */
	uint32 Calls = 0;
	/** The platform's event counters (FPlatformTime::NumPerfCounters of them) inside the scopes. */
	uint32 Events[MaxPerfCounters] = {};
};

static_assert(FPlatformTime::NumPerfCounters <= FCycleStatCounts::MaxPerfCounters, "Too many perf counters");

/** A node of the tree: a stat under a parent (Leon). */
struct FCycleStatNode
{
	/** Null for the root, the frame. */
	const FCycleStat* Stat = nullptr;
	int16 Parent = INDEX_NONE;
	int16 FirstChild = INDEX_NONE;
	int16 NextSibling = INDEX_NONE;
	/** 0 for the root, 1 for the frame's top level scopes. */
	int16 Depth = 0;
	FCycleStatCounts Counts;
};

/**
 * The game thread's cycle stats (UE: FThreadStats, the collector of a thread, and FStats::AdvanceFrame): the switch,
 * the scopes and the tree.
 */
class CORE_API FThreadStats
{
public:
	static constexpr int32 MaxNodes = 256;
	static constexpr int32 MaxDepth = 16;

	/** Whether the scopes record (UE): something holds the stats enabled. */
	[[nodiscard]] static bool IsCollectingData()
	{
		return MasterEnableCounter > 0;
	}

	/**
	 * Counts a holder of the stats in or out (UE: MasterEnableAdd / MasterEnableSubtract): `-LogFrameTimes` for the
	 * run, the `stat cycles` page while it shows. The platform's event counters run while one holds them.
	 */
	static void MasterEnableAdd(int32 Value = 1);
	static void MasterEnableSubtract(int32 Value = 1);

	/**
	 * A frame starts (UE: FStats::AdvanceFrame), outside every scope: the root counts it and the cycles since the last
	 * one, then the event counters restart from zero, so they never reach their overflow bit.
	 */
	static void AdvanceFrame();

	/** Opens a scope of Stat, and closes the innermost open one (FScopeCycleCounter). */
	static void Enter(FCycleStat& Stat);
	static void Exit();

	/** The tree: node 0 is the root, a parent comes before its children. */
	[[nodiscard]] static int32 GetNumNodes();
	[[nodiscard]] static const FCycleStatNode& GetNode(int32 Index);
	/** Changes with every Reset, so a window knows its start is stale. */
	[[nodiscard]] static uint32 GetResetSerial();

	/** Forgets the nodes, their counts and the warnings; the scopes still open record nothing (tests). */
	static void Reset();

private:
	static int32 MasterEnableCounter;
};

/** Times its block into a stat while the stats collect (UE: FScopeCycleCounter); SCOPE_CYCLE_COUNTER makes one. */
class FScopeCycleCounter
{
public:
	FORCEINLINE explicit FScopeCycleCounter(TStatId StatId)
	{
		if (FThreadStats::IsCollectingData() && StatId.IsValidStat())
		{
			FThreadStats::Enter(*StatId.GetRawPointer());
			bRecording = true;
		}
	}
	FORCEINLINE ~FScopeCycleCounter()
	{
		if (bRecording)
		{
			FThreadStats::Exit();
		}
	}
	FScopeCycleCounter(const FScopeCycleCounter&) = delete;
	FScopeCycleCounter& operator=(const FScopeCycleCounter&) = delete;

private:
	/** Entered: the scope ends even if the stats stopped collecting inside it. */
	bool bRecording = false;
};

/**
 * The cycle stats over a stretch of frames (Leon): the tree's counts now less those at Restart. `-LogFrameTimes`' 5 s
 * log, its run summary and the `stat cycles` page each keep one.
 */
class CORE_API FCycleStatsWindow
{
public:
	/** The window starts now. */
	void Restart();

	/** The frames that started since Restart. */
	[[nodiscard]] int64 GetNumFrames() const;

	/** A node's counts over the window (a node added since Restart counts from zero). */
	[[nodiscard]] FCycleStatCounts GetNodeCounts(int32 NodeIndex) const;

	/** A stat's counts over the window: its nodes summed (a stat may run under several parents). */
	[[nodiscard]] FCycleStatCounts GetStatCounts(TStatId StatId) const;

	/** Milliseconds per frame of Cycles counted over the window (0 without frames). */
	[[nodiscard]] double GetMillisecondsPerFrame(uint64 Cycles) const;

	/**
	 * The hierarchy, a line per node in the tree's order (children after their parent, indented two spaces a level):
	 * "<Description>: <ms> ms, <calls> calls" per frame, and the platform's event counters per frame on the top level.
	 * A node under MinMilliseconds per frame or deeper than MaxLineDepth is left out with its children, and after
	 * MaxLines lines the rest.
	 */
	void GetReportLines(TArray<FString>& OutLines, double MinMilliseconds = 0.0,
		int32 MaxLineDepth = FThreadStats::MaxDepth, int32 MaxLines = FThreadStats::MaxNodes) const;

private:
	TArray<FCycleStatCounts> Start;
	uint32 StartResetSerial = 0;
};

/** A stats group (UE: the FStatGroup_<Id> type). GroupCat (UE's STATCAT_Advanced) is not used. */
#define DECLARE_STATS_GROUP(GroupDesc, GroupId, GroupCat)                                                              \
	struct FStatGroup_##GroupId                                                                                        \
	{                                                                                                                  \
		static constexpr const TCHAR* Description = GroupDesc;                                                         \
		static constexpr const TCHAR* GroupName = TEXT(#GroupId);                                                      \
	}

/** A cycle stat of this .cpp (UE). */
#define DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)                                                               \
	static FCycleStat StatId##_CycleStat(CounterName, TEXT(#StatId), FStatGroup_##GroupId::GroupName)

/** A cycle stat in a header (UE), defined in one .cpp with DEFINE_STAT. */
#define DECLARE_CYCLE_STAT_EXTERN(CounterName, StatId, GroupId, API)                                                   \
	struct FStat_##StatId                                                                                              \
	{                                                                                                                  \
		static constexpr const TCHAR* Description = CounterName;                                                       \
		static constexpr const TCHAR* GroupName = FStatGroup_##GroupId::GroupName;                                     \
	};                                                                                                                 \
	extern API FCycleStat StatId##_CycleStat

#define DEFINE_STAT(StatId)                                                                                            \
	FCycleStat StatId##_CycleStat(FStat_##StatId::Description, TEXT(#StatId), FStat_##StatId::GroupName)

#define GET_STATID(StatId) (TStatId(&StatId##_CycleStat))

/** Times the rest of the block into StatId while the stats collect (UE). */
#define SCOPE_CYCLE_COUNTER(StatId) FScopeCycleCounter CycleCount_##StatId(GET_STATID(StatId))

// The engine's groups (UE declares them in Stats2.h too).
DECLARE_STATS_GROUP(TEXT("Engine"), STATGROUP_Engine, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Game"), STATGROUP_Game, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("AI"), STATGROUP_AI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Collision"), STATGROUP_Collision, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Physics"), STATGROUP_Physics, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Audio"), STATGROUP_Audio, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Scene Rendering"), STATGROUP_SceneRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("RHI"), STATGROUP_RHI, STATCAT_Advanced);
