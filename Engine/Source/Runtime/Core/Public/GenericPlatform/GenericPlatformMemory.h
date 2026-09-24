#pragma once

#include "HAL/Platform.h"

/** Process memory numbers (UE: FGenericPlatformMemoryStats, reduced). */
struct CORE_API FGenericPlatformMemoryStats
{
	/** Resident physical memory used by the process (working set / program image + heap). */
	uint64 UsedPhysical = 0;

	/** Committed virtual memory (private bytes); equals UsedPhysical where there is no VM. */
	uint64 UsedVirtual = 0;

	/** Physical memory available to the process; 0 when unknown. */
	uint64 TotalPhysical = 0;
};

typedef FGenericPlatformMemoryStats FPlatformMemoryStats;

/** Memory queries; platforms implement GetStats (UE: FGenericPlatformMemory). */
struct CORE_API FGenericPlatformMemory
{
	static FPlatformMemoryStats GetStats();
};
