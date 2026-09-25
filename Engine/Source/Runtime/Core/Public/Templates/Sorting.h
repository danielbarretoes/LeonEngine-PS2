#pragma once

#include "Algo/IntroSort.h"
#include "Algo/StableSort.h"
#include "CoreTypes.h"
#include "Templates/Less.h"

/**
 * Passes the predicate the dereferenced elements when sorting an array of pointers, like UE: TArray<T*>::Sort()
 * orders by *A < *B (UE: TDereferenceWrapper).
 */
template <typename T, class PredicateType>
struct TDereferenceWrapper
{
	const PredicateType& Predicate;

	TDereferenceWrapper(const PredicateType& InPredicate)
		: Predicate(InPredicate)
	{
	}

	FORCEINLINE bool operator()(T& A, T& B)
	{
		return Predicate(A, B);
	}
	FORCEINLINE bool operator()(const T& A, const T& B) const
	{
		return Predicate(A, B);
	}
};

template <typename T, class PredicateType>
struct TDereferenceWrapper<T*, PredicateType>
{
	const PredicateType& Predicate;

	TDereferenceWrapper(const PredicateType& InPredicate)
		: Predicate(InPredicate)
	{
	}

	FORCEINLINE bool operator()(T* A, T* B) const
	{
		return Predicate(*A, *B);
	}
};

/** A raw range for the Algo functions (UE: TArrayRange). */
template <typename T>
struct TArrayRange
{
	TArrayRange(T* InPtr, int32 InSize)
		: Begin(InPtr)
		, Size(InSize)
	{
	}

	T* GetData() const
	{
		return Begin;
	}
	int32 Num() const
	{
		return Size;
	}

private:
	T* Begin;
	int32 Size;
};

template <typename T>
struct TIsContiguousContainer<TArrayRange<T>>
{
	enum
	{
		Value = true
	};
};

/** Unstable sort; pointer elements are compared through the pointer (UE: Sort). */
template <class T, class PredicateType>
FORCEINLINE void Sort(T* First, const int32 Num, const PredicateType& Predicate)
{
	Algo::IntroSort(TArrayRange<T>(First, Num), TDereferenceWrapper<T, PredicateType>(Predicate));
}

template <class T, class PredicateType>
FORCEINLINE void Sort(T** First, const int32 Num, const PredicateType& Predicate)
{
	Algo::IntroSort(TArrayRange<T*>(First, Num), TDereferenceWrapper<T*, PredicateType>(Predicate));
}

template <class T>
FORCEINLINE void Sort(T* First, const int32 Num)
{
	Algo::IntroSort(TArrayRange<T>(First, Num), TDereferenceWrapper<T, TLess<T>>(TLess<T>()));
}

template <class T>
FORCEINLINE void Sort(T** First, const int32 Num)
{
	Algo::IntroSort(TArrayRange<T*>(First, Num), TDereferenceWrapper<T*, TLess<T>>(TLess<T>()));
}

/** Stable sort; pointer elements are compared through the pointer (UE: StableSort). */
template <class T, class PredicateType>
FORCEINLINE void StableSort(T* First, const int32 Num, const PredicateType& Predicate)
{
	Algo::StableSort(TArrayRange<T>(First, Num), TDereferenceWrapper<T, PredicateType>(Predicate));
}

template <class T, class PredicateType>
FORCEINLINE void StableSort(T** First, const int32 Num, const PredicateType& Predicate)
{
	Algo::StableSort(TArrayRange<T*>(First, Num), TDereferenceWrapper<T*, PredicateType>(Predicate));
}

template <class T>
FORCEINLINE void StableSort(T* First, const int32 Num)
{
	Algo::StableSort(TArrayRange<T>(First, Num), TDereferenceWrapper<T, TLess<T>>(TLess<T>()));
}

template <class T>
FORCEINLINE void StableSort(T** First, const int32 Num)
{
	Algo::StableSort(TArrayRange<T*>(First, Num), TDereferenceWrapper<T*, TLess<T>>(TLess<T>()));
}
