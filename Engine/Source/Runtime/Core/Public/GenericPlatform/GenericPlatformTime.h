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
};
