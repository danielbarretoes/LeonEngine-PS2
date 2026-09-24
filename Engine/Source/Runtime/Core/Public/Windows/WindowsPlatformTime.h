#pragma once

#include "GenericPlatform/GenericPlatformTime.h"

struct CORE_API FWindowsPlatformTime : public FGenericPlatformTime
{
	static uint64 Cycles64();
	static double GetSecondsPerCycle64();
	static uint64 CyclesToMicroseconds(uint64 Cycles);
	static double Seconds();
};

typedef FWindowsPlatformTime FPlatformTime;
