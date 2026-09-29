#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

/** What a timer calls (UE: FTimerDelegate): a UObject's method, a lambda, a static function. */
DECLARE_DELEGATE(FTimerDelegate);

/**
 * The world's timers (UE: FTimerManager, the part Leon uses): SetTimer calls a delegate once, or every Rate seconds
 * with bLoop, after FirstDelay (Rate when negative); ClearTimer stops it; IsTimerActive, GetTimerRemaining and the rest
 * ask. UWorld owns one (UWorld::GetTimerManager, AActor::GetWorldTimerManager) and ticks it first in each world step,
 * right after the world's time moves on (Leon; UE ticks it after TG_PostPhysics): what a timer changes (a reload done,
 * a round gone live) is what the step's actors see, as when each of them polled its deadline in its own tick.
 *
 * - The clock is integer: TimeUnitsPerSecond units a second (3 MHz), so a step of 1/30, 1/25, 1/50 or 1/60 s is a
 *   whole number of units and a timer comes on the same step every run, whatever it adds up to (D4: no float sums).
 * - A timer fires its delay after the clock's time when it was set (in a world, the step's time): at the first Tick
 *   whose time reaches it. Between two steps the clock holds the last step's time.
 * - Due timers fire in the order they are due, and those due at the same time in the order they were set. A looping
 *   timer that fell several periods behind fires once per period in that Tick (UE), and keeps its phase.
 * - A callback may set or clear any timer, its own included: cleared, it does not come back; set again on the same
 *   handle, it is a new timer. A timer whose UObject went away (BindUObject) is dropped without a call.
 * - Setting and firing allocate nothing but the delegate's binding (FTimerDelegate::CreateUObject); a looping timer
 *   keeps its binding.
 */
class ENGINE_API FTimerManager
{
public:
	/** The units of the timers' clock in a second. */
	static constexpr uint64 TimeUnitsPerSecond = 3000000;

	/** Seconds as the clock's units (rounded; negative is 0). */
	[[nodiscard]] static uint64 SecondsToTimeUnits(float Seconds);
	/** The clock's units as seconds. */
	[[nodiscard]] static float TimeUnitsToSeconds(uint64 Units);

	FTimerManager() = default;
	~FTimerManager() = default;

	FTimerManager(const FTimerManager&) = delete;
	FTimerManager& operator=(const FTimerManager&) = delete;

	/** Moves the clock by DeltaTime and fires what is due (UE: Tick). */
	void Tick(float DeltaTime);

	/**
	 * Sets a timer calling InTimerMethod on InObj (UE: SetTimer): once after InFirstDelay (InRate when negative), then
	 * every InRate seconds with InbLoop. A valid InOutHandle's timer is cleared first; a rate of 0 or less only clears
	 * it. InOutHandle names the new timer.
	 */
	template <class UserClass>
	void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj,
		typename TMemFunPtrType<false, UserClass, void()>::Type InTimerMethod, float InRate, bool InbLoop = false,
		float InFirstDelay = -1.0f)
	{
		InternalSetTimer(
			InOutHandle, FTimerDelegate::CreateUObject(InObj, InTimerMethod), InRate, InbLoop, InFirstDelay, true);
	}
	template <class UserClass>
	void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj,
		typename TMemFunPtrType<true, UserClass, void()>::Type InTimerMethod, float InRate, bool InbLoop = false,
		float InFirstDelay = -1.0f)
	{
		InternalSetTimer(
			InOutHandle, FTimerDelegate::CreateUObject(InObj, InTimerMethod), InRate, InbLoop, InFirstDelay, true);
	}
	/** SetTimer with a delegate (a lambda: FTimerDelegate::CreateLambda). */
	void SetTimer(FTimerHandle& InOutHandle, const FTimerDelegate& InDelegate, float InRate, bool InbLoop,
		float InFirstDelay = -1.0f);
	/** A timer that calls nothing: only its time is asked for (UE). */
	void SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay = -1.0f);

	/** Calls InDelegate once at the next Tick (UE: SetTimerForNextTick). */
	FTimerHandle SetTimerForNextTick(const FTimerDelegate& InDelegate);
	template <class UserClass>
	FTimerHandle SetTimerForNextTick(UserClass* InObj, typename TMemFunPtrType<false, UserClass, void()>::Type InMethod)
	{
		return SetTimerForNextTick(FTimerDelegate::CreateUObject(InObj, InMethod));
	}

	/** Stops the timer and invalidates the handle (UE: ClearTimer). */
	void ClearTimer(FTimerHandle& InHandle);
	/** Stops every timer bound to Object (UE: ClearAllTimersForObject). */
	void ClearAllTimersForObject(const void* Object);
	/** Stops every timer (the world's teardown). */
	void ClearAllTimers();

	/**
	 * Whether the timer will still fire (UE: IsTimerActive): set and not run out or cleared. Inside its own callback a
	 * looping timer is active and a one-shot one is not.
	 */
	[[nodiscard]] bool IsTimerActive(FTimerHandle InHandle) const;
	/** Whether the handle still names a timer that will fire (UE: TimerExists). */
	[[nodiscard]] bool TimerExists(FTimerHandle InHandle) const
	{
		return IsTimerActive(InHandle);
	}
	/** Seconds until it fires, -1 without the timer (UE: GetTimerRemaining). */
	[[nodiscard]] float GetTimerRemaining(FTimerHandle InHandle) const;
	/** Seconds since it was set or last fired, -1 without the timer (UE: GetTimerElapsed). */
	[[nodiscard]] float GetTimerElapsed(FTimerHandle InHandle) const;
	/** Its rate in seconds, -1 without the timer (UE: GetTimerRate). */
	[[nodiscard]] float GetTimerRate(FTimerHandle InHandle) const;

	/** The clock, in units (TimeUnitsPerSecond a second). */
	[[nodiscard]] uint64 GetTimeUnits() const
	{
		return InternalTime;
	}

	/** Timers set and not yet run out or cleared (tests). */
	[[nodiscard]] int32 GetNumActiveTimers() const;

private:
	enum class ETimerStatus : uint8
	{
		Free,
		Active,
		/** Its callback runs. */
		Executing,
		/** Cleared while in the heap: dropped when it comes up. */
		ActivePendingRemoval,
	};

	struct FTimerData
	{
		FTimerDelegate Delegate;
		/** The clock's units it fires at. */
		uint64 ExpireTime = 0;
		/** The period, in units. */
		uint64 Rate = 0;
		/** Set order: fires first among timers due at the same time. */
		uint64 SetOrder = 0;
		/** The handle that names it (FTimerHandle::Handle), 0 when free. */
		uint64 Handle = 0;
		ETimerStatus Status = ETimerStatus::Free;
		bool bLoop = false;
		/** It had a delegate: once that no longer is bound, the timer is dropped. */
		bool bRequiresDelegate = false;
	};

	void InternalSetTimerUnits(FTimerHandle& InOutHandle, FTimerDelegate&& InDelegate, uint64 Rate, uint64 FirstDelay,
		bool InbLoop, bool bRequiresDelegate);
	void InternalSetTimer(FTimerHandle& InOutHandle, FTimerDelegate&& InDelegate, float InRate, bool InbLoop,
		float InFirstDelay, bool bRequiresDelegate);
	[[nodiscard]] const FTimerData* FindTimer(FTimerHandle InHandle) const;
	[[nodiscard]] FTimerData* FindTimer(FTimerHandle InHandle);
	/** Frees a slot (its serial number moves on, so old handles miss it). */
	void FreeTimer(int32 Index);
	[[nodiscard]] bool HeapLess(int32 A, int32 B) const;
	void HeapPush(int32 Index);
	[[nodiscard]] int32 HeapPop();

	/** The timers' slots; free ones are reused. */
	TArray<FTimerData> Timers;
	TArray<int32> FreeIndices;
	/** Each slot's serial number, part of its handles. */
	TArray<uint32> SerialNumbers;
	/** The active timers, a min-heap by (ExpireTime, SetOrder). */
	TArray<int32> ActiveTimerHeap;

	uint64 InternalTime = 0;
	uint64 NextSetOrder = 0;
};
