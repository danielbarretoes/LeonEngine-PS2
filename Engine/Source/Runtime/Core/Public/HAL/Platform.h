#pragma once

#include "HAL/PreprocessorHelpers.h"

// Every platform macro defaults to 0; LeonBuildTool defines the current one to 1 (PLATFORM_<NAME>=1).
#ifndef PLATFORM_WINDOWS
	#define PLATFORM_WINDOWS 0
#endif
#ifndef PLATFORM_LINUX
	#define PLATFORM_LINUX 0
#endif
#ifndef PLATFORM_PS2
	#define PLATFORM_PS2 0
#endif

#include "GenericPlatform/GenericPlatform.h"
#include COMPILED_PLATFORM_HEADER(Platform.h)

// Capability defaults; a platform header overrides the ones that apply to it.
#ifndef PLATFORM_DESKTOP
	#define PLATFORM_DESKTOP 0
#endif
#ifndef PLATFORM_64BITS
	#define PLATFORM_64BITS 0
#endif
#ifndef PLATFORM_LITTLE_ENDIAN
	#define PLATFORM_LITTLE_ENDIAN 1
#endif
#ifndef FORCEINLINE
	#define FORCEINLINE inline
#endif
#ifndef FORCENOINLINE
	#define FORCENOINLINE
#endif
#ifndef FORCEINLINE_DEBUGGABLE
	#define FORCEINLINE_DEBUGGABLE inline
#endif

// Line end written to text files (UE: LINE_TERMINATOR).
#ifndef LINE_TERMINATOR
	#define LINE_TERMINATOR "\n"
#endif

// Branch prediction hints (UE: LIKELY / UNLIKELY).
#ifndef LIKELY
	#if defined(__GNUC__) || defined(__clang__)
		#define LIKELY(x) __builtin_expect(!!(x), 1)
		#define UNLIKELY(x) __builtin_expect(!!(x), 0)
	#else
		#define LIKELY(x) (x)
		#define UNLIKELY(x) (x)
	#endif
#endif

// Stops in the debugger (UE: PLATFORM_BREAK); callers check FPlatformMisc::IsDebuggerPresent first.
#ifndef PLATFORM_BREAK
	#define PLATFORM_BREAK() __builtin_trap()
#endif

// printf-style format checking on GCC / Clang (UE: PRINTF_FORMAT_STRING-like annotations).
#if defined(__GNUC__) || defined(__clang__)
	#define LEON_PRINTF_FORMAT(FormatIndex, FirstArgIndex) __attribute__((format(printf, FormatIndex, FirstArgIndex)))
#else
	#define LEON_PRINTF_FORMAT(FormatIndex, FirstArgIndex)
#endif

// Global fixed-width types (UE: HAL/Platform.h).
typedef FPlatformTypes::uint8 uint8;
typedef FPlatformTypes::uint16 uint16;
typedef FPlatformTypes::uint32 uint32;
typedef FPlatformTypes::uint64 uint64;
typedef FPlatformTypes::int8 int8;
typedef FPlatformTypes::int16 int16;
typedef FPlatformTypes::int32 int32;
typedef FPlatformTypes::int64 int64;
typedef FPlatformTypes::ANSICHAR ANSICHAR;
typedef FPlatformTypes::WIDECHAR WIDECHAR;
typedef FPlatformTypes::TCHAR TCHAR;
typedef FPlatformTypes::SIZE_T SIZE_T;
typedef FPlatformTypes::PTRINT PTRINT;
typedef FPlatformTypes::UPTRINT UPTRINT;

static_assert(sizeof(int8) == 1 && sizeof(uint8) == 1, "8-bit types must be 1 byte");
static_assert(sizeof(int16) == 2 && sizeof(uint16) == 2, "16-bit types must be 2 bytes");
static_assert(sizeof(int32) == 4 && sizeof(uint32) == 4, "32-bit types must be 4 bytes");
static_assert(sizeof(int64) == 8 && sizeof(uint64) == 8, "64-bit types must be 8 bytes");
static_assert(sizeof(PTRINT) == sizeof(void*) && sizeof(UPTRINT) == sizeof(void*), "PTRINT must be pointer sized");
static_assert(sizeof(TCHAR) == 1, "TCHAR is UTF-8 on every platform");

/** String literal of TCHARs (UE: TEXT). TCHAR is UTF-8, so a literal is already a TCHAR string. */
#define TEXT(x) x
