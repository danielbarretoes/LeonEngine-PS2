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

	/** Seconds since an arbitrary epoch. */
	static double Seconds();

	/** Local calendar time (UE: SystemTime). DayOfWeek is 0 for Sunday. */
	static void SystemTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);

	/** UTC calendar time (UE: UtcTime). */
	static void UtcTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};
