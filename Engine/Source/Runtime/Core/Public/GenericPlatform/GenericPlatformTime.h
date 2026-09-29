#pragma once

#include "HAL/Platform.h"

/** High-resolution time (UE: FGenericPlatformTime). Platforms implement the cycle counter. */
struct CORE_API FGenericPlatformTime
{
	/** Monotonic cycle counter of the platform's high-resolution timer. */
	static uint64 Cycles64();

	/** Seconds per Cycles64() tick. */
	static double GetSecondsPerCycle64();

	/** Integer conversion (Leon extension): EE code avoids double math in per-frame paths. */
	static uint64 CyclesToMicroseconds(uint64 Cycles);

	/**
	 * A 32-bit cycle counter for short intervals (UE: Cycles), what the cycle stats read at every scope: take the
	 * difference of two readings as a uint32, so a wrap between them does not matter (a platform's wrap period bounds
	 * the longest interval: the EE's CPU clock wraps every 14.6 s).
	 */
	static uint32 Cycles();

	/** Seconds per Cycles() tick (UE: GetSecondsPerCycle). */
	static double GetSecondsPerCycle();

	/**
	 * The CPU's event counters the cycle stats read beside Cycles() (Leon; the EE's PCR0 / PCR1 count its instruction
	 * and data cache misses). A platform without them has none: the stats compile the reads away.
	 */
	static constexpr int32 NumPerfCounters = 0;

	/** Starts (bEnable) or stops the event counters, zeroed. */
	static void EnablePerfCounters(bool /*bEnable*/)
	{
	}

	/** Zeroes the running event counters (once a frame, so they never reach their overflow bit). */
	static void ResetPerfCounters()
	{
	}

	/** Event counter Index (0 .. NumPerfCounters - 1) now; take differences of two readings as a uint32. */
	static uint32 ReadPerfCounter(int32 /*Index*/)
	{
		return 0;
	}

	/** What event counter Index counts, a short identifier ("icache_misses"): the stats' logs use it as a key. */
	static const TCHAR* GetPerfCounterName(int32 /*Index*/)
	{
		return TEXT("");
	}

	/** Seconds since an arbitrary epoch. */
	static double Seconds();

	/** Local calendar time (UE: SystemTime). DayOfWeek is 0 for Sunday. */
	static void SystemTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);

	/** UTC calendar time (UE: UtcTime). */
	static void UtcTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};
