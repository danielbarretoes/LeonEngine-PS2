#include "HAL/PlatformMemory.h"

#include <cstddef>

// newlib provides sbrk, but <unistd.h> hides it under strict -std=c++17 (no GNU extensions).
extern "C" void* sbrk(std::ptrdiff_t Increment);

namespace
{
	constexpr UPTRINT EEUserBase = 0x00100000u;
	constexpr uint64 EERamBytes = 32ull * 1024ull * 1024ull;
}

FPlatformMemoryStats FPS2PlatformMemory::GetStats()
{
	// Program image + heap high-water (newlib break). The stack at the top of RAM is not counted.
	const UPTRINT Break = reinterpret_cast<UPTRINT>(sbrk(0));
	FPlatformMemoryStats Stats;
	Stats.UsedPhysical = Break > EEUserBase ? static_cast<uint64>(Break - EEUserBase) : 0;
	Stats.UsedVirtual = Stats.UsedPhysical;
	Stats.TotalPhysical = EERamBytes;
	return Stats;
}
