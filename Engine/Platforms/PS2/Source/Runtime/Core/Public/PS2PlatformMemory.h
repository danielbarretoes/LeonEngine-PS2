#pragma once

#include "GenericPlatform/GenericPlatformMemory.h"

/** EE main RAM: 32 MB; the kernel owns the first 1 MB and ELFs load at 0x00100000. */
struct CORE_API FPS2PlatformMemory : public FGenericPlatformMemory
{
	static FPlatformMemoryStats GetStats();

	/** The EE's scratchpad: 16 KB of on-chip RAM at 0x70000000 (the SDK's linkfile names it .spad and puts nothing
	 * there). */
	static uint8* GetOnChipScratchpad()
	{
		return reinterpret_cast<uint8*>(0x70000000u);
	}
};

typedef FPS2PlatformMemory FPlatformMemory;
