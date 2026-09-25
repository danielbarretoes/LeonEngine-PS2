#pragma once

#include "GenericPlatform/GenericPlatformAtomics.h"

#include <intrin.h>

/** MSVC interlocked intrinsics (UE: FWindowsPlatformAtomics); int32 and int64 only. */
struct FWindowsPlatformAtomics : public FGenericPlatformAtomics
{
	static FORCEINLINE int32 InterlockedIncrement(volatile int32* Value)
	{
		return static_cast<int32>(_InterlockedIncrement(reinterpret_cast<volatile long*>(Value))) - 1;
	}
	static FORCEINLINE int64 InterlockedIncrement(volatile int64* Value)
	{
		return _InterlockedIncrement64(reinterpret_cast<volatile long long*>(Value)) - 1;
	}

	static FORCEINLINE int32 InterlockedDecrement(volatile int32* Value)
	{
		return static_cast<int32>(_InterlockedDecrement(reinterpret_cast<volatile long*>(Value))) + 1;
	}
	static FORCEINLINE int64 InterlockedDecrement(volatile int64* Value)
	{
		return _InterlockedDecrement64(reinterpret_cast<volatile long long*>(Value)) + 1;
	}

	static FORCEINLINE int32 InterlockedAdd(volatile int32* Value, int32 Amount)
	{
		return static_cast<int32>(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(Value), Amount));
	}
	static FORCEINLINE int64 InterlockedAdd(volatile int64* Value, int64 Amount)
	{
		return _InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(Value), Amount);
	}

	static FORCEINLINE int32 InterlockedExchange(volatile int32* Value, int32 Exchange)
	{
		return static_cast<int32>(_InterlockedExchange(reinterpret_cast<volatile long*>(Value), Exchange));
	}
	static FORCEINLINE int64 InterlockedExchange(volatile int64* Value, int64 Exchange)
	{
		return _InterlockedExchange64(reinterpret_cast<volatile long long*>(Value), Exchange);
	}

	static FORCEINLINE int32 InterlockedCompareExchange(volatile int32* Dest, int32 Exchange, int32 Comparand)
	{
		return static_cast<int32>(
			_InterlockedCompareExchange(reinterpret_cast<volatile long*>(Dest), Exchange, Comparand));
	}
	static FORCEINLINE int64 InterlockedCompareExchange(volatile int64* Dest, int64 Exchange, int64 Comparand)
	{
		return _InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(Dest), Exchange, Comparand);
	}

	static FORCEINLINE void* InterlockedCompareExchangePointer(void* volatile* Dest, void* Exchange, void* Comparand)
	{
		return _InterlockedCompareExchangePointer(Dest, Exchange, Comparand);
	}

	static FORCEINLINE int32 AtomicRead(volatile const int32* Source)
	{
		return InterlockedCompareExchange(const_cast<volatile int32*>(Source), 0, 0);
	}
	static FORCEINLINE int64 AtomicRead(volatile const int64* Source)
	{
		return InterlockedCompareExchange(const_cast<volatile int64*>(Source), 0, 0);
	}

	static FORCEINLINE void AtomicStore(volatile int32* Dest, int32 Value)
	{
		InterlockedExchange(Dest, Value);
	}
	static FORCEINLINE void AtomicStore(volatile int64* Dest, int64 Value)
	{
		InterlockedExchange(Dest, Value);
	}
};

typedef FWindowsPlatformAtomics FPlatformAtomics;
