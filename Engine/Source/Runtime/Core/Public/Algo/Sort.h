#pragma once

#include "Algo/IntroSort.h"

namespace Algo
{
	/** Unstable sort of a contiguous range with operator< (UE: Algo::Sort). */
	template <typename RangeType>
	FORCEINLINE void Sort(RangeType&& Range)
	{
		IntroSort(Forward<RangeType>(Range));
	}

	template <typename RangeType, typename PredicateType>
	FORCEINLINE void Sort(RangeType&& Range, PredicateType Pred)
	{
		IntroSort(Forward<RangeType>(Range), MoveTemp(Pred));
	}

	template <typename RangeType, typename ProjectionType>
	FORCEINLINE void SortBy(RangeType&& Range, ProjectionType Proj)
	{
		IntroSortBy(Forward<RangeType>(Range), MoveTemp(Proj));
	}

	template <typename RangeType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void SortBy(RangeType&& Range, ProjectionType Proj, PredicateType Pred)
	{
		IntroSortBy(Forward<RangeType>(Range), MoveTemp(Proj), MoveTemp(Pred));
	}
} // namespace Algo
