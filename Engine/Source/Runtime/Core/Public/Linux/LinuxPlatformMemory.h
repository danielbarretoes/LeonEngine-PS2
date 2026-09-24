#pragma once

#include "GenericPlatform/GenericPlatformMemory.h"

struct CORE_API FLinuxPlatformMemory : public FGenericPlatformMemory
{
	static FPlatformMemoryStats GetStats();
};

typedef FLinuxPlatformMemory FPlatformMemory;
