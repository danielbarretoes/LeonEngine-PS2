#include "HAL/PlatformTime.h"

#include <Windows.h>

namespace
{
	uint64 QueryFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return static_cast<uint64>(Frequency.QuadPart);
	}

	uint64 CyclesPerSecond()
	{
		static const uint64 Frequency = QueryFrequency();
		return Frequency;
	}
} // namespace

uint64 FWindowsPlatformTime::Cycles64()
{
	LARGE_INTEGER Counter;
	QueryPerformanceCounter(&Counter);
	return static_cast<uint64>(Counter.QuadPart);
}

double FWindowsPlatformTime::GetSecondsPerCycle64()
{
	return 1.0 / static_cast<double>(CyclesPerSecond());
}

uint64 FWindowsPlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	return (Cycles / CyclesPerSecond()) * 1000000ull + (Cycles % CyclesPerSecond()) * 1000000ull / CyclesPerSecond();
}

double FWindowsPlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}
