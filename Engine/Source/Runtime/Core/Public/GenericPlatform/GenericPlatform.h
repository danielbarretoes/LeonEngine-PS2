#pragma once

/**
 * Generic type table. Each platform derives F<Platform>PlatformTypes from it and typedefs it as
 * FPlatformTypes; HAL/Platform.h then exposes the global int32/uint8/… typedefs.
 */
struct CORE_API FGenericPlatformTypes
{
	// Unsigned base types.
	typedef unsigned char uint8;
	typedef unsigned short int uint16;
	typedef unsigned int uint32;
	typedef unsigned long long uint64;

	// Signed base types.
	typedef signed char int8;
	typedef signed short int int16;
	typedef signed int int32;
	typedef signed long long int64;

	// Character types.
	typedef char ANSICHAR;

	// Pointer-sized integers (platforms override for their pointer width).
	typedef decltype(sizeof(0)) SIZE_T;
	typedef long PTRINT;
	typedef unsigned long UPTRINT;
};
