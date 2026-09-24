#pragma once

#include "GenericPlatform/GenericPlatformMemory.h"

/** EE main RAM: 32 MB; the kernel owns the first 1 MB and ELFs load at 0x00100000. */
struct CORE_API FPS2PlatformMemory : public FGenericPlatformMemory
{
	static FPlatformMemoryStats GetStats();
};

typedef FPS2PlatformMemory FPlatformMemory;
