#pragma once

/** Windows (Win64) platform types and capabilities. Included through HAL/Platform.h. */
struct CORE_API FWindowsPlatformTypes : public FGenericPlatformTypes
{
	typedef unsigned long long SIZE_T;
	typedef long long PTRINT;
	typedef unsigned long long UPTRINT;
};

typedef FWindowsPlatformTypes FPlatformTypes;

#define PLATFORM_DESKTOP 1
#define PLATFORM_64BITS 1
#define PLATFORM_LITTLE_ENDIAN 1
#define LINE_TERMINATOR "\r\n"

#define FORCEINLINE __forceinline
#define FORCENOINLINE __declspec(noinline)
#define FORCEINLINE_DEBUGGABLE inline
#define PLATFORM_BREAK() __debugbreak()
