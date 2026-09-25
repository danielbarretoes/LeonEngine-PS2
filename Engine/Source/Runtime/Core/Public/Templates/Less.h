#pragma once

#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"

/** Binary predicate A < B (UE: TLess). TLess<> deduces the operand types; the default argument is in ContainersFwd.h.
 */
template <typename T /* = void */>
struct TLess
{
	FORCEINLINE bool operator()(const T& A, const T& B) const
	{
		return A < B;
	}
};

template <>
struct TLess<void>
{
	template <typename T>
	FORCEINLINE bool operator()(const T& A, const T& B) const
	{
		return A < B;
	}
};
