#pragma once

#include "CoreTypes.h"

#include <utility>

/** Compile-time sequence of integers (UE: TIntegerSequence). */
template <typename T, T... Indices>
struct TIntegerSequence
{
};

namespace UE::Core::Private
{
	template <typename T, typename StdSequence>
	struct TIntegerSequenceFromStd;

	template <typename T, T... Indices>
	struct TIntegerSequenceFromStd<T, std::integer_sequence<T, Indices...>>
	{
		using Type = TIntegerSequence<T, Indices...>;
	};
} // namespace UE::Core::Private

/** TIntegerSequence<T, 0, 1, ..., N - 1> (UE: TMakeIntegerSequence). */
template <typename T, T N>
using TMakeIntegerSequence =
	typename UE::Core::Private::TIntegerSequenceFromStd<T, std::make_integer_sequence<T, N>>::Type;
