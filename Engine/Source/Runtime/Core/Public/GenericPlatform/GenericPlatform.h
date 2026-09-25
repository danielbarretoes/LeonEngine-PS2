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

	// Character types. TCHAR is UTF-8 on every platform (Leon deviation from UE's UTF-16 TCHAR on Windows);
	// WIDECHAR only appears inside the Windows HAL.
	typedef char ANSICHAR;
	typedef wchar_t WIDECHAR;
	typedef ANSICHAR TCHAR;

	// Pointer-sized integers (platforms override for their pointer width).
	typedef decltype(sizeof(0)) SIZE_T;
	typedef long PTRINT;
	typedef unsigned long UPTRINT;
};
