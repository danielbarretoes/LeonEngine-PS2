#pragma once

#include "Algo/BinarySearch.h"
#include "CoreTypes.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Templates/UnrealTemplate.h"

namespace AlgoImpl
{
	/** Reverses [First, First + Num). */
	template <typename T>
	FORCEINLINE void ReverseInternal(T* First, int32 Num)
	{
		for (int32 Index = 0, Last = Num - 1; Index < Last; ++Index, --Last)
		{
			Swap(First[Index], First[Last]);
		}
	}

	/** Rotates [First, First + Num) left by Count elements. */
	template <typename T>
	FORCEINLINE void RotateInternal(T* First, int32 Num, int32 Count)
	{
		if (Count && Count != Num)
		{
			ReverseInternal(First, Count);
			ReverseInternal(First + Count, Num - Count);
			ReverseInternal(First, Num);
		}
	}

	/** In-place stable merge of the sorted runs [First, First + Mid) and [First + Mid, First + Num) (UE: Merge). */
	template <typename T, typename ProjectionType, typename PredicateType>
	void Merge(T* First, int32 Mid, int32 Num, ProjectionType Projection, PredicateType Predicate)
	{
		int32 AStart = 0;
		int32 BStart = Mid;

		while (AStart < BStart && BStart < Num)
		{
			// Index after the last value == First[BStart].
			int32 NewAOffset = AlgoImpl::UpperBoundInternal(
				First + AStart, BStart - AStart, Invoke(Projection, First[BStart]), Projection, Predicate);
			AStart += NewAOffset;

			if (AStart >= BStart)
			{
				break;
			}

			// Index of the first value == First[AStart].
			int32 NewBOffset = AlgoImpl::LowerBoundInternal(
				First + BStart, Num - BStart, Invoke(Projection, First[AStart]), Projection, Predicate);
			RotateInternal(First + AStart, NewBOffset + BStart - AStart, BStart - AStart);
			BStart += NewBOffset;
			AStart += NewBOffset + 1;
		}
	}

	constexpr int32 MinMergeSubgroupSize = 2;

	/** Bottom-up stable merge sort without extra memory (UE: StableSortInternal). */
	template <typename T, typename ProjectionType, typename PredicateType>
	void StableSortInternal(T* First, int32 Num, ProjectionType Projection, PredicateType Predicate)
	{
		int32 SubgroupStart = 0;

		if constexpr (MinMergeSubgroupSize > 1)
		{
			if constexpr (MinMergeSubgroupSize > 2)
			{
				// Sort small subgroups with insertion sort.
				do
				{
					int32 GroupEnd = SubgroupStart + MinMergeSubgroupSize;
					if (Num < GroupEnd)
					{
						GroupEnd = Num;
					}
					for (int32 Index = SubgroupStart + 1; Index < GroupEnd; ++Index)
					{
						for (int32 Inner = Index; Inner > SubgroupStart &&
							Predicate(Invoke(Projection, First[Inner]), Invoke(Projection, First[Inner - 1]));
							--Inner)
						{
							Swap(First[Inner], First[Inner - 1]);
						}
					}
					SubgroupStart += MinMergeSubgroupSize;
				} while (SubgroupStart < Num);
			}
			else
			{
				for (; SubgroupStart < Num - 1; SubgroupStart += 2)
				{
					if (Predicate(
							Invoke(Projection, First[SubgroupStart + 1]), Invoke(Projection, First[SubgroupStart])))
					{
						Swap(First[SubgroupStart], First[SubgroupStart + 1]);
					}
				}
			}
		}

		int32 SubgroupSize = MinMergeSubgroupSize;
		while (SubgroupSize < Num)
		{
			SubgroupStart = 0;
			do
			{
				const int32 Mid = SubgroupSize;
				int32 MergeNum = SubgroupSize << 1;
				if (Num - SubgroupStart < MergeNum)
				{
					MergeNum = Num - SubgroupStart;
				}
				if (Mid < MergeNum)
				{
					Merge(First + SubgroupStart, Mid, MergeNum, Projection, Predicate);
				}
				SubgroupStart += SubgroupSize << 1;
			} while (SubgroupStart < Num);

			SubgroupSize <<= 1;
		}
	}
} // namespace AlgoImpl

namespace Algo
{
	/** Stable in-place sort of a contiguous range (UE: Algo::StableSort). */
	template <typename RangeType>
	FORCEINLINE void StableSort(RangeType&& Range)
	{
		AlgoImpl::StableSortInternal(GetData(Range), int32(GetNum(Range)), FIdentityFunctor(), TLess<>());
	}

	template <typename RangeType, typename PredicateType>
	FORCEINLINE void StableSort(RangeType&& Range, PredicateType Pred)
	{
		AlgoImpl::StableSortInternal(GetData(Range), int32(GetNum(Range)), FIdentityFunctor(), MoveTemp(Pred));
	}

	template <typename RangeType, typename ProjectionType>
	FORCEINLINE void StableSortBy(RangeType&& Range, ProjectionType Proj)
	{
		AlgoImpl::StableSortInternal(GetData(Range), int32(GetNum(Range)), MoveTemp(Proj), TLess<>());
	}

	template <typename RangeType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void StableSortBy(RangeType&& Range, ProjectionType Proj, PredicateType Pred)
	{
		AlgoImpl::StableSortInternal(GetData(Range), int32(GetNum(Range)), MoveTemp(Proj), MoveTemp(Pred));
	}
} // namespace Algo
