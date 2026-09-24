#include "HAL/PlatformTime.h"

#include <timer.h>

namespace
{
	constexpr uint64 BusClockHz = 147456000ull;
}

uint64 FPS2PlatformTime::Cycles64()
{
	return GetTimerSystemTime();
}

double FPS2PlatformTime::GetSecondsPerCycle64()
{
	return 1.0 / static_cast<double>(BusClockHz);
}

uint64 FPS2PlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	// 1e6 / 147456000 = 125 / 18432 (exact, integer only).
	return (Cycles * 125ull) / 18432ull;
}

double FPS2PlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}
