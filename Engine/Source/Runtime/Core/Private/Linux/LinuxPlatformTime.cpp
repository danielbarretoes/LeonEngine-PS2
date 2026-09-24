#include "HAL/PlatformTime.h"

#include <time.h>

uint64 FLinuxPlatformTime::Cycles64()
{
	timespec Now;
	clock_gettime(CLOCK_MONOTONIC, &Now);
	return static_cast<uint64>(Now.tv_sec) * 1000000000ull + static_cast<uint64>(Now.tv_nsec);
}

double FLinuxPlatformTime::GetSecondsPerCycle64()
{
	return 1.0e-9;
}

uint64 FLinuxPlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	return Cycles / 1000ull;
}

double FLinuxPlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}
