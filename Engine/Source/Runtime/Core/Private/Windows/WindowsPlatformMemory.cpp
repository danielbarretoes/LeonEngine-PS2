#include "HAL/PlatformMemory.h"

#include <Windows.h>
#include <psapi.h>

FPlatformMemoryStats FWindowsPlatformMemory::GetStats()
{
	FPlatformMemoryStats Stats;
	PROCESS_MEMORY_COUNTERS_EX Counters{};
	Counters.cb = sizeof(Counters);
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&Counters),
			sizeof(Counters)))
	{
		Stats.UsedPhysical = static_cast<uint64>(Counters.WorkingSetSize);
		Stats.UsedVirtual = static_cast<uint64>(Counters.PrivateUsage);
	}

	MEMORYSTATUSEX Status{};
	Status.dwLength = sizeof(Status);
	if (GlobalMemoryStatusEx(&Status))
	{
		Stats.TotalPhysical = static_cast<uint64>(Status.ullTotalPhys);
	}
	return Stats;
}
