#pragma once

#include "CoreTypes.h"
#include "HAL/MemoryBase.h"

#include <cstring>
#include <type_traits>

/** Memory functions (UE: FMemory). Heap calls go to GMalloc. */
struct CORE_API FMemory
{
	static FORCEINLINE void* Memmove(void* Dest, const void* Src, SIZE_T Count)
	{
		return std::memmove(Dest, Src, Count);
	}

	static FORCEINLINE int32 Memcmp(const void* Buf1, const void* Buf2, SIZE_T Count)
	{
		return std::memcmp(Buf1, Buf2, Count);
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return std::memset(Dest, Char, Count);
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return std::memset(Dest, 0, Count);
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return std::memcpy(Dest, Src, Count);
	}

	/** Memcpy for large blocks (same as Memcpy on Leon platforms). */
	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return std::memcpy(Dest, Src, Count);
	}

	template <class T>
	static FORCEINLINE void Memset(T& Src, uint8 ValueToSet)
	{
		static_assert(!std::is_pointer_v<T>, "For pointers use the three parameters function");
		Memset(&Src, ValueToSet, sizeof(T));
	}

	template <class T>
	static FORCEINLINE void Memzero(T& Src)
	{
		static_assert(!std::is_pointer_v<T>, "For pointers use the two parameters function");
		Memzero(&Src, sizeof(T));
	}

	template <class T>
	static FORCEINLINE void Memcpy(T& Dest, const T& Src)
	{
		static_assert(!std::is_pointer_v<T>, "For pointers use the three parameters function");
		Memcpy(&Dest, &Src, sizeof(T));
	}

	static void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void Free(void* Original);
	static void* MallocZeroed(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);

	/** Usable size of an allocation (0 when unknown). */
	static SIZE_T GetAllocSize(void* Original);

	static SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);

	/** Current / peak bytes of GMalloc (Leon extension; the stats overlay shows it). */
	static FMallocUsage GetUsage();
};
