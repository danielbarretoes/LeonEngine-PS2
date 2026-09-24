#pragma once

/**
 * PlayStation 2 Emotion Engine platform types and capabilities (platform extension).
 * EE code is ILP32: int, long and pointers are 32 bits; long long is 64 bits.
 */
struct FPS2PlatformTypes : public FGenericPlatformTypes
{
	typedef unsigned int SIZE_T;
	typedef int PTRINT;
	typedef unsigned int UPTRINT;
};

typedef FPS2PlatformTypes FPlatformTypes;

#define PLATFORM_DESKTOP 0
#define PLATFORM_64BITS 0
#define PLATFORM_LITTLE_ENDIAN 1

#define FORCEINLINE inline __attribute__((always_inline))
#define FORCENOINLINE __attribute__((noinline))
