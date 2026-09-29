#pragma once

#include "GenericPlatform/GenericPlatformTime.h"

struct CORE_API FWindowsPlatformTime : public FGenericPlatformTime
{
	static uint64 Cycles64();
	static double GetSecondsPerCycle64();
	/** The low 32 bits of Cycles64 (UE: Cycles): the performance counter's ticks. */
	static uint32 Cycles();
	static double GetSecondsPerCycle();
	static uint64 CyclesToMicroseconds(uint64 Cycles);
	static double Seconds();
	static void SystemTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static void UtcTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};

typedef FWindowsPlatformTime FPlatformTime;
