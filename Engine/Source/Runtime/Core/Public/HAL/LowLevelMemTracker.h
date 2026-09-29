#pragma once

#include "CoreTypes.h"
#include "HAL/PreprocessorHelpers.h"
#include "Misc/Build.h"

/**
 * Low-level memory tracking (UE: LLM, HAL/LowLevelMemTracker.h; Docs/PLANS/ps2-shipping.md N17): every GMalloc block is
 * charged to the tag of the innermost LLM_SCOPE open when it was allocated (EngineMisc outside every scope), and each
 * tag has a current and a peak.
 *
 * - Budgets come from the config ([Core.MemoryBudgets] of the Engine config, in KB: one key per tag name and `Total`;
 *   PS2Engine.ini sets the EE's). A tag, or the total, that grows over WarningPercent of its budget logs a warning
 * once; over the budget is a fatal error that names the tag (the plan: a hard budget is fatal and says which tag broke
 * it). A test sets a hook that receives the events instead (SetBudgetHook).
 * - A scope costs two stores; an allocation adds to its tag and compares with the tag's next threshold.
 * - One thread (Leon's engine): the active tag is a plain global.
 * - Compiled out of Shipping (ENABLE_LOW_LEVEL_MEM_TRACKER 0, as UE): the scopes are empty and nothing is charged.
 */
#ifndef ENABLE_LOW_LEVEL_MEM_TRACKER
	#define ENABLE_LOW_LEVEL_MEM_TRACKER !UE_BUILD_SHIPPING
#endif

/** What a block of memory is for (UE: ELLMTag, a reduced set; the names are UE's where UE has one). */
enum class ELLMTag : uint8
{
	/** Everything outside a scope: start-up, the engine's services, the containers of the frame. */
	EngineMisc,
	/** The UObjects themselves (StaticAllocateObject) and the reflection's classes. */
	UObject,
	/** A package's bytes while it loads (FLinkerLoad's load arena) (UE: LoadMapMisc). */
	LoadMapMisc,
	/** Texture data: the assets' texels and the GS texture cache. */
	Textures,
	/** Static and skeletal mesh data. */
	Meshes,
	/** Animation sequences and poses. */
	Animation,
	/** Sound waves and the audio device. */
	Audio,
	/** The physics scene: bodies, shapes, queries. */
	Physics,
	/** Navigation, AI controllers and perception (Leon). */
	AI,
	/** The scene, its proxies and the scene renderer (UE: SceneRender). */
	SceneRender,
	/** The gameplay tick: actors and their components (Leon; UE: a project's own tags). */
	GameMisc,
	/** The chunks of the per-frame FMemStack (Leon). */
	Temporary,
	/**
	 * The GS command lists (their writes, copied images, vertex draws and batches, skinned palettes) and the frame's
	 * DMA chains (Leon, Docs/PLANS/ps2-shipping.md N14b): they grow with the triangles a frame draws.
	 */
	RenderLists,

	Count
};

/** What crossed a budget line (FLowLevelMemTracker::SetBudgetHook). */
enum class ELLMBudgetEvent : uint8
{
	/** Over WarningPercent of the budget. */
	Warning,
	/** Over the budget: fatal without a hook. */
	Exceeded
};

/** A tag's numbers (Leon). */
struct FLLMTagStats
{
	uint64 CurrentBytes = 0;
	uint64 PeakBytes = 0;
	/** Allocations charged to the tag since start-up. */
	uint64 TotalAllocations = 0;
	/** The hard budget; 0 without one. */
	uint64 BudgetBytes = 0;
};

/** The tracker (UE: FLowLevelMemTracker): the tag stack, the counts per tag and the budgets. */
class CORE_API FLowLevelMemTracker
{
public:
	static constexpr int32 NumTags = int32(ELLMTag::Count);

	/** Receives a budget event instead of the warning log or the fatal error (tests). */
	using FBudgetHook = void (*)(ELLMBudgetEvent Event, const TCHAR* TagName, uint64 CurrentBytes, uint64 BudgetBytes);

	/** Whether this build tracks (ENABLE_LOW_LEVEL_MEM_TRACKER). */
	[[nodiscard]] static constexpr bool IsEnabled()
	{
		return ENABLE_LOW_LEVEL_MEM_TRACKER != 0;
	}

	/** "UObject", ...; "Total" for ELLMTag::Count (the budget of the whole heap). */
	[[nodiscard]] static const TCHAR* GetTagName(ELLMTag Tag);

	/** The tag that new allocations are charged to: the innermost LLM_SCOPE's. */
	[[nodiscard]] static FORCEINLINE ELLMTag GetActiveTag()
	{
		return ActiveTag;
	}

	/** FLLMScope's store. */
	static FORCEINLINE void SetActiveTag(ELLMTag Tag)
	{
		ActiveTag = Tag;
	}

	/**
	 * Charges Bytes to Tag (the allocator, under its lock). True when the tag or the total went over its next
	 * threshold: the allocator then calls ReportBudgets outside its lock.
	 */
	static FORCEINLINE bool OnAlloc(ELLMTag Tag, uint64 Bytes)
	{
		FTagState& State = Tags[int32(Tag)];
		State.Stats.CurrentBytes += Bytes;
		if (State.Stats.CurrentBytes > State.Stats.PeakBytes)
		{
			State.Stats.PeakBytes = State.Stats.CurrentBytes;
		}
		++State.Stats.TotalAllocations;
		FTagState& Total = Tags[NumTags];
		Total.Stats.CurrentBytes += Bytes;
		if (Total.Stats.CurrentBytes > Total.Stats.PeakBytes)
		{
			Total.Stats.PeakBytes = Total.Stats.CurrentBytes;
		}
		++Total.Stats.TotalAllocations;
		return State.Stats.CurrentBytes > State.ThresholdBytes || Total.Stats.CurrentBytes > Total.ThresholdBytes;
	}

	/** Gives Bytes back to Tag (the allocator, under its lock). */
	static FORCEINLINE void OnFree(ELLMTag Tag, uint64 Bytes)
	{
		Tags[int32(Tag)].Stats.CurrentBytes -= Bytes;
		Tags[NumTags].Stats.CurrentBytes -= Bytes;
	}

	/** Logs the warnings and raises the fatal errors of Tag and of the total (or calls the hook). */
	static void ReportBudgets(ELLMTag Tag);

	/** A tag's numbers; ELLMTag::Count gives the total's. */
	[[nodiscard]] static FLLMTagStats GetTagStats(ELLMTag Tag);

	/** Sets a tag's hard budget in bytes (0: none); ELLMTag::Count sets the total's. Re-arms its warning. */
	static void SetBudget(ELLMTag Tag, uint64 Bytes);

	/** The share of a budget over which a warning is logged (90 by default). Re-arms every warning. */
	static void SetWarningPercent(int32 Percent);
	[[nodiscard]] static int32 GetWarningPercent();

	/** Reads the budgets from [Core.MemoryBudgets] of the Engine config (KB per tag name, Total, WarningPercent). */
	static void LoadBudgetsFromConfig();

	/** Installs a hook for the budget events (nullptr: log and fail as usual); returns the previous one. */
	static FBudgetHook SetBudgetHook(FBudgetHook Hook);

private:
	struct FTagState
	{
		FLLMTagStats Stats;
		/** The next line to watch: the warning, then the budget, then none (~0). */
		uint64 ThresholdBytes = ~uint64(0);
		bool bWarned = false;
		bool bExceeded = false;
	};

	static void UpdateThreshold(FTagState& State);
	static void ReportTag(ELLMTag Tag);

	static ELLMTag ActiveTag;
	static int32 WarningPercent;
	static FBudgetHook BudgetHook;
	static bool bReporting;
	/** One per tag, then the total. */
	static FTagState Tags[NumTags + 1];
};

/** Charges the allocations of the rest of the block to a tag (UE: FLLMScope; LLM_SCOPE makes one). */
class FLLMScope
{
public:
	FORCEINLINE explicit FLLMScope(ELLMTag Tag)
		: Previous(FLowLevelMemTracker::GetActiveTag())
	{
		FLowLevelMemTracker::SetActiveTag(Tag);
	}
	FORCEINLINE ~FLLMScope()
	{
		FLowLevelMemTracker::SetActiveTag(Previous);
	}
	FLLMScope(const FLLMScope&) = delete;
	FLLMScope& operator=(const FLLMScope&) = delete;

private:
	ELLMTag Previous;
};

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	/** Charges the allocations of the rest of the block to Tag (UE: LLM_SCOPE(ELLMTag::X)). */
	#define LLM_SCOPE(Tag) FLLMScope PREPROCESSOR_JOIN(LLMScope, __LINE__)(Tag)
#else
	#define LLM_SCOPE(Tag)
#endif
