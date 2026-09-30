#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

/**
 * A world's tick functions (UE: FTickTaskManager with its per-level FTickTaskLevel, on one thread and without the task
 * graph): for each ETickingGroup the list of the enabled registered tick functions, in their tick order
 * (FTickFunction::MakeTickOrder), which UWorld::Tick runs group by group (RunTickGroup).
 *
 * Only what ticks is visited: a disabled tick function leaves its list and one that cannot tick never registers.
 * While a group runs, a tick function enabled ahead of the one running ticks in this step, one enabled behind it from
 * the next step, and one disabled or unregistered is skipped (its slot is compacted when the group ends). A tick
 * function whose prerequisite of the same group is still due waits for it and runs right after it, in list order;
 * whatever still waits when the group ends (its prerequisite went away) runs then.
 */
class ENGINE_API FTickTaskManager
{
public:
	FTickTaskManager() = default;
	~FTickTaskManager();

	FTickTaskManager(const FTickTaskManager&) = delete;
	FTickTaskManager& operator=(const FTickTaskManager&) = delete;

	/** A new step: the tick functions' states of the last one are forgotten (UE: StartFrame). */
	void StartFrame();

	/**
	 * Runs a group's due tick functions (UE: RunTickGroup); a LEVELTICK_PauseTick step runs only the ones that tick
	 * when paused (FTickFunction::bTickEvenWhenPaused).
	 */
	void RunTickGroup(ETickingGroup Group, float DeltaSeconds, ELevelTick TickType = LEVELTICK_All);

	/** How many enabled tick functions a group has (tests, stats). */
	[[nodiscard]] int32 GetNumEnabledTickFunctions(ETickingGroup Group) const;

	/** The steps started (StartFrame). */
	[[nodiscard]] uint32 GetFrameCounter() const
	{
		return FrameCounter;
	}

	/** Takes every tick function out (the world's teardown). */
	void UnregisterAll();

private:
	friend struct FTickFunction;

	void AddTickFunction(FTickFunction& TickFunction);
	void RemoveTickFunction(FTickFunction& TickFunction);
	/** An enabled registered tick function joins its group's list; a disabled one leaves it. */
	void OnTickFunctionEnableChanged(FTickFunction& TickFunction);
	[[nodiscard]] uint64 NextSerialTickOrder()
	{
		return ++NextSerial;
	}

	void InsertInList(FTickFunction& TickFunction);
	void RemoveFromList(FTickFunction& TickFunction);
	/** Whether a tick function is due this step (its interval), deciding it once per step. */
	[[nodiscard]] bool IsDueThisFrame(FTickFunction& TickFunction, float DeltaSeconds);
	/** The first prerequisite that is still due in this group this step, or null when it may run. */
	[[nodiscard]] FTickFunction* FindPendingPrerequisite(FTickFunction& TickFunction, float DeltaSeconds);
	/** A waiting tick function is still in its list and due in this step. */
	[[nodiscard]] bool CanRunWaiting(const FTickFunction& TickFunction) const;
	void Execute(FTickFunction& TickFunction, float DeltaSeconds, ELevelTick TickType);
	/** Runs what waited for Finished and may run now (in waiting order), and so on for what those finish. */
	void ReleaseWaiting(FTickFunction& Finished, float DeltaSeconds, ELevelTick TickType);

	/** Every registered tick function, enabled or not (UnregisterAll). */
	TArray<FTickFunction*> AllTickFunctions;

	/** The enabled tick functions of each group, in tick order; a null slot is one that left while its group ran. */
	TArray<FTickFunction*> Enabled[TG_MAX];

	/** A tick function waiting for a prerequisite, and that prerequisite. */
	struct FWaiting
	{
		FTickFunction* TickFunction;
		FTickFunction* Blocker;
	};
	/** The running group's waiting tick functions, in the order they started to wait. */
	TArray<FWaiting> Waiting;

	/** The group running and the index of its tick function running (the list may change meanwhile). */
	int32 RunningGroup = -1;
	int32 RunningIndex = -1;
	/** What the running group's step runs (LEVELTICK_PauseTick: only what ticks when paused). */
	ELevelTick RunningTickType = LEVELTICK_All;
	/** A tick function left its list while its group ran: the list is compacted when the group ends. */
	bool bHasNullSlots = false;

	uint32 FrameCounter = 0;
	uint64 NextSerial = 0;
};
