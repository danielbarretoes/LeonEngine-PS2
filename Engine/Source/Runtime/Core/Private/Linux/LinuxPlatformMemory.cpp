#include "HAL/PlatformMemory.h"

#include <fstream>
#include <unistd.h>

FPlatformMemoryStats FLinuxPlatformMemory::GetStats()
{
	FPlatformMemoryStats Stats;
	std::ifstream Statm("/proc/self/statm");
	uint64 SizePages = 0;
	uint64 ResidentPages = 0;
	const long PageSize = sysconf(_SC_PAGESIZE);
	if ((Statm >> SizePages >> ResidentPages) && PageSize > 0)
	{
		Stats.UsedPhysical = ResidentPages * static_cast<uint64>(PageSize);
		Stats.UsedVirtual = SizePages * static_cast<uint64>(PageSize);
	}
	const long PhysPages = sysconf(_SC_PHYS_PAGES);
	if (PhysPages > 0 && PageSize > 0)
	{
		Stats.TotalPhysical = static_cast<uint64>(PhysPages) * static_cast<uint64>(PageSize);
	}
	return Stats;
}
