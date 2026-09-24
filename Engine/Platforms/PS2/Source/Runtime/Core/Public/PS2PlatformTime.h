#pragma once

#include "GenericPlatform/GenericPlatformTime.h"

/** EE timer: GetTimerSystemTime() counts BUSCLK cycles (147.456 MHz). */
struct CORE_API FPS2PlatformTime : public FGenericPlatformTime
{
	static uint64 Cycles64();
	static double GetSecondsPerCycle64();
	static uint64 CyclesToMicroseconds(uint64 Cycles);
	static double Seconds();
};

typedef FPS2PlatformTime FPlatformTime;
