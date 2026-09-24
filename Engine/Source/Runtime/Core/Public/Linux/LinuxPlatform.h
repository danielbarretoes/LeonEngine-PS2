#pragma once

/** Linux (x86_64) platform types and capabilities. Included through HAL/Platform.h. */
struct FLinuxPlatformTypes : public FGenericPlatformTypes
{
	typedef unsigned long SIZE_T;
	typedef long PTRINT;
	typedef unsigned long UPTRINT;
};

typedef FLinuxPlatformTypes FPlatformTypes;

#define PLATFORM_DESKTOP 1
#define PLATFORM_64BITS 1
#define PLATFORM_LITTLE_ENDIAN 1

#define FORCEINLINE inline __attribute__((always_inline))
#define FORCENOINLINE __attribute__((noinline))
