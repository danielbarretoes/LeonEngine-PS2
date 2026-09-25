#pragma once

#include "CoreTypes.h"

/** Binary predicate A > B (UE: TGreater). TGreater<> deduces the operand types. */
template <typename T = void>
struct TGreater
{
	FORCEINLINE bool operator()(const T& A, const T& B) const
	{
		return B < A;
	}
};

template <>
struct TGreater<void>
{
	template <typename T>
	FORCEINLINE bool operator()(const T& A, const T& B) const
	{
		return B < A;
	}
};
