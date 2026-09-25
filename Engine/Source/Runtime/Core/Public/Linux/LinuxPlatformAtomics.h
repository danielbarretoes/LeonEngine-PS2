#pragma once

#include "GenericPlatform/GenericPlatformAtomics.h"

/** GCC / Clang __atomic builtins, sequentially consistent (UE: FClangPlatformAtomics). */
struct FLinuxPlatformAtomics : public FGenericPlatformAtomics
{
	template <typename T>
	static FORCEINLINE T InterlockedIncrement(volatile T* Value)
	{
		return __atomic_fetch_add(Value, 1, __ATOMIC_SEQ_CST);
	}

	template <typename T>
	static FORCEINLINE T InterlockedDecrement(volatile T* Value)
	{
		return __atomic_fetch_sub(Value, 1, __ATOMIC_SEQ_CST);
	}

	template <typename T>
	static FORCEINLINE T InterlockedAdd(volatile T* Value, T Amount)
	{
		return __atomic_fetch_add(Value, Amount, __ATOMIC_SEQ_CST);
	}

	template <typename T>
	static FORCEINLINE T InterlockedExchange(volatile T* Value, T Exchange)
	{
		return __atomic_exchange_n(Value, Exchange, __ATOMIC_SEQ_CST);
	}

	template <typename T>
	static FORCEINLINE T InterlockedCompareExchange(volatile T* Dest, T Exchange, T Comparand)
	{
		__atomic_compare_exchange_n(Dest, &Comparand, Exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		return Comparand;
	}

	static FORCEINLINE void* InterlockedCompareExchangePointer(void* volatile* Dest, void* Exchange, void* Comparand)
	{
		__atomic_compare_exchange_n(Dest, &Comparand, Exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		return Comparand;
	}

	template <typename T>
	static FORCEINLINE T AtomicRead(volatile const T* Source)
	{
		return __atomic_load_n(Source, __ATOMIC_SEQ_CST);
	}

	template <typename T>
	static FORCEINLINE void AtomicStore(volatile T* Dest, T Value)
	{
		__atomic_store_n(Dest, Value, __ATOMIC_SEQ_CST);
	}
};

typedef FLinuxPlatformAtomics FPlatformAtomics;
