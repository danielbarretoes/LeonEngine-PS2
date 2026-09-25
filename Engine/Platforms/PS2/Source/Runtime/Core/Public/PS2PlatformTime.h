#pragma once

#include "GenericPlatform/GenericPlatformTime.h"

/** EE timer: GetTimerSystemTime() counts BUSCLK cycles (147.456 MHz). */
struct CORE_API FPS2PlatformTime : public FGenericPlatformTime
{
	static uint64 Cycles64();
	static double GetSecondsPerCycle64();
	static uint64 CyclesToMicroseconds(uint64 Cycles);
	static double Seconds();
	static void SystemTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static void UtcTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};

typedef FPS2PlatformTime FPlatformTime;
