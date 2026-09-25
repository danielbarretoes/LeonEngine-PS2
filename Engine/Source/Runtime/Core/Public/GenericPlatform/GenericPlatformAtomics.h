#pragma once

#include "HAL/Platform.h"

/**
 * Interlocked operations (UE: FGenericPlatformAtomics). Each returns the value the destination held before
 * the operation. This generic version is NOT atomic: it serves single-threaded platforms (PS2: Leon runs a
 * single EE thread, and the R5900 has no 64-bit LL/SC). Threaded platforms override every function.
 */
struct FGenericPlatformAtomics
{
	template <typename T>
	static FORCEINLINE T InterlockedIncrement(volatile T* Value)
	{
		return (*Value)++;
	}

	template <typename T>
	static FORCEINLINE T InterlockedDecrement(volatile T* Value)
	{
		return (*Value)--;
	}

	template <typename T>
	static FORCEINLINE T InterlockedAdd(volatile T* Value, T Amount)
	{
		const T Previous = *Value;
		*Value = Previous + Amount;
		return Previous;
	}

	template <typename T>
	static FORCEINLINE T InterlockedExchange(volatile T* Value, T Exchange)
	{
		const T Previous = *Value;
		*Value = Exchange;
		return Previous;
	}

	template <typename T>
	static FORCEINLINE T InterlockedCompareExchange(volatile T* Dest, T Exchange, T Comparand)
	{
		const T Previous = *Dest;
		if (Previous == Comparand)
		{
			*Dest = Exchange;
		}
		return Previous;
	}

	static FORCEINLINE void* InterlockedCompareExchangePointer(void* volatile* Dest, void* Exchange, void* Comparand)
	{
		void* Previous = *Dest;
		if (Previous == Comparand)
		{
			*Dest = Exchange;
		}
		return Previous;
	}

	template <typename T>
	static FORCEINLINE T AtomicRead(volatile const T* Source)
	{
		return *Source;
	}

	template <typename T>
	static FORCEINLINE void AtomicStore(volatile T* Dest, T Value)
	{
		*Dest = Value;
	}
};
