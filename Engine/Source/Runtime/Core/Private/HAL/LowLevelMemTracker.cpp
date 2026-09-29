#include "HAL/LowLevelMemTracker.h"

#include "Logging/LogMacros.h"
#include "Misc/ConfigCacheIni.h"

ELLMTag FLowLevelMemTracker::ActiveTag = ELLMTag::EngineMisc;
int32 FLowLevelMemTracker::WarningPercent = 90;
FLowLevelMemTracker::FBudgetHook FLowLevelMemTracker::BudgetHook = nullptr;
bool FLowLevelMemTracker::bReporting = false;
FLowLevelMemTracker::FTagState FLowLevelMemTracker::Tags[FLowLevelMemTracker::NumTags + 1];

namespace
{
	/** The config section of the budgets. */
	constexpr const TCHAR* BudgetSection = TEXT("Core.MemoryBudgets");

	const TCHAR* const TagNames[] = {TEXT("EngineMisc"), TEXT("UObject"), TEXT("LoadMapMisc"), TEXT("Textures"),
		TEXT("Meshes"), TEXT("Animation"), TEXT("Audio"), TEXT("Physics"), TEXT("AI"), TEXT("SceneRender"),
		TEXT("GameMisc"), TEXT("Temporary"), TEXT("RenderLists"), TEXT("Total")};
	static_assert(UE_ARRAY_COUNT(TagNames) == FLowLevelMemTracker::NumTags + 1, "A name per tag, then Total");

	[[nodiscard]] unsigned long long ToKilobytes(uint64 Bytes)
	{
		return static_cast<unsigned long long>((Bytes + 1023) / 1024);
	}
} // namespace

const TCHAR* FLowLevelMemTracker::GetTagName(ELLMTag Tag)
{
	const int32 Index = int32(Tag);
	return Index >= 0 && Index <= NumTags ? TagNames[Index] : TEXT("?");
}

void FLowLevelMemTracker::UpdateThreshold(FTagState& State)
{
	const uint64 Budget = State.Stats.BudgetBytes;
	if (Budget == 0 || State.bExceeded)
	{
		State.ThresholdBytes = ~uint64(0);
	}
	else if (!State.bWarned)
	{
		State.ThresholdBytes = Budget / 100 * uint64(WarningPercent) + Budget % 100 * uint64(WarningPercent) / 100;
	}
	else
	{
		State.ThresholdBytes = Budget;
	}
}

void FLowLevelMemTracker::ReportTag(ELLMTag Tag)
{
	FTagState& State = Tags[int32(Tag)];
	const uint64 Current = State.Stats.CurrentBytes;
	const uint64 Budget = State.Stats.BudgetBytes;
	if (Budget == 0 || Current <= State.ThresholdBytes)
	{
		return;
	}
	const TCHAR* Name = GetTagName(Tag);
	if (Current > Budget)
	{
		// Past the warning too: one event, the one that matters.
		State.bWarned = true;
		State.bExceeded = true;
		UpdateThreshold(State);
		if (BudgetHook != nullptr)
		{
			BudgetHook(ELLMBudgetEvent::Exceeded, Name, Current, Budget);
			return;
		}
		UE_LOG(LogMemory, Fatal, "Memory budget exceeded: %s uses %llu KB of its %llu KB budget ([%s] %s)", Name,
			ToKilobytes(Current), ToKilobytes(Budget), BudgetSection, Name);
		return;
	}
	State.bWarned = true;
	UpdateThreshold(State);
	if (BudgetHook != nullptr)
	{
		BudgetHook(ELLMBudgetEvent::Warning, Name, Current, Budget);
		return;
	}
	UE_LOG(LogMemory, Warning, "Memory: %s uses %llu KB, over %d%% of its %llu KB budget", Name, ToKilobytes(Current),
		WarningPercent, ToKilobytes(Budget));
}

void FLowLevelMemTracker::ReportBudgets(ELLMTag Tag)
{
	// The log allocates, and those allocations cross the same line: they are not reported again.
	if (bReporting)
	{
		return;
	}
	bReporting = true;
	ReportTag(Tag);
	ReportTag(ELLMTag::Count);
	bReporting = false;
}

FLLMTagStats FLowLevelMemTracker::GetTagStats(ELLMTag Tag)
{
	const int32 Index = int32(Tag);
	return Index >= 0 && Index <= NumTags ? Tags[Index].Stats : FLLMTagStats();
}

void FLowLevelMemTracker::SetBudget(ELLMTag Tag, uint64 Bytes)
{
	const int32 Index = int32(Tag);
	if (Index < 0 || Index > NumTags)
	{
		return;
	}
	FTagState& State = Tags[Index];
	State.Stats.BudgetBytes = Bytes;
	State.bWarned = false;
	State.bExceeded = false;
	UpdateThreshold(State);
}

void FLowLevelMemTracker::SetWarningPercent(int32 Percent)
{
	WarningPercent = Percent < 1 ? 1 : (Percent > 100 ? 100 : Percent);
	for (FTagState& State : Tags)
	{
		State.bWarned = false;
		State.bExceeded = false;
		UpdateThreshold(State);
	}
}

int32 FLowLevelMemTracker::GetWarningPercent()
{
	return WarningPercent;
}

void FLowLevelMemTracker::LoadBudgetsFromConfig()
{
	if (GConfig == nullptr)
	{
		return;
	}
	int32 Percent = WarningPercent;
	(void)GConfig->GetInt(BudgetSection, TEXT("WarningPercent"), Percent, GEngineIni);
	SetWarningPercent(Percent);
	int32 NumBudgets = 0;
	for (int32 Index = 0; Index <= NumTags; ++Index)
	{
		int32 Kilobytes = 0;
		(void)GConfig->GetInt(BudgetSection, TagNames[Index], Kilobytes, GEngineIni);
		SetBudget(ELLMTag(Index), Kilobytes > 0 ? uint64(Kilobytes) * 1024 : 0);
		NumBudgets += Kilobytes > 0 ? 1 : 0;
	}
	if (NumBudgets > 0)
	{
		UE_LOG(LogMemory, Log, "Memory budgets: %d from [%s], warnings over %d%%%s", NumBudgets, BudgetSection,
			WarningPercent, IsEnabled() ? "" : " (not tracked in this build)");
	}
	// A budget below what is already allocated is reported now.
	for (int32 Index = 0; Index < NumTags; ++Index)
	{
		ReportBudgets(ELLMTag(Index));
	}
}

FLowLevelMemTracker::FBudgetHook FLowLevelMemTracker::SetBudgetHook(FBudgetHook Hook)
{
	const FBudgetHook Previous = BudgetHook;
	BudgetHook = Hook;
	return Previous;
}
