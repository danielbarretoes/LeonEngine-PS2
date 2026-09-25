#pragma once

#include "Algo/Impl/BinaryHeap.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Templates/UnrealTemplate.h"

namespace AlgoImpl
{
	/** Quicksort with a median-ish pivot, bubble sort for short runs and heap sort past the depth limit (UE). */
	template <typename T, typename ProjectionType, typename PredicateType>
	void IntroSortInternal(T* First, SIZE_T Num, ProjectionType Projection, PredicateType Predicate)
	{
		struct FStack
		{
			T* Min;
			T* Max;
			uint32 MaxDepth;
		};

		if (Num < 2)
		{
			return;
		}

		FStack RecursionStack[32] = {{First, First + Num - 1, FMath::FloorLog2(uint32(Num)) * 2u + 2u}};
		FStack Current;
		FStack Inner;
		for (FStack* StackTop = RecursionStack; StackTop >= RecursionStack; --StackTop)
		{
			Current = *StackTop;

		Loop:
			const PTRINT Count = Current.Max - Current.Min + 1;

			if (Current.MaxDepth == 0)
			{
				// We're too deep into quick sort, switch to heap sort.
				HeapSortInternal(Current.Min, int32(Count), Projection, Predicate);
				continue;
			}

			if (Count <= 8)
			{
				// Use simple bubble-sort.
				while (Current.Max > Current.Min)
				{
					T* Max = Current.Min;
					for (T* Item = Current.Min + 1; Item <= Current.Max; Item++)
					{
						if (Predicate(Invoke(Projection, *Max), Invoke(Projection, *Item)))
						{
							Max = Item;
						}
					}
					Swap(*Max, *Current.Max--);
				}
			}
			else
			{
				// Grab the middle element so presorted input does not hit the worst case.
				Swap(Current.Min[Count / 2], Current.Min[0]);

				// Divide the list into two halves: items <= Current.Min and items > Current.Min.
				Inner.Min = Current.Min;
				Inner.Max = Current.Max + 1;
				for (;;)
				{
					while (++Inner.Min <= Current.Max &&
						!Predicate(Invoke(Projection, *Current.Min), Invoke(Projection, *Inner.Min)))
					{
					}
					while (--Inner.Max > Current.Min &&
						!Predicate(Invoke(Projection, *Inner.Max), Invoke(Projection, *Current.Min)))
					{
					}
					if (Inner.Min > Inner.Max)
					{
						break;
					}
					Swap(*Inner.Min, *Inner.Max);
				}
				Swap(*Current.Min, *Inner.Max);

				--Current.MaxDepth;

				// Save the big half and recurse with the small half.
				if (Inner.Max - 1 - Current.Min >= Current.Max - Inner.Min)
				{
					if (Current.Min + 1 < Inner.Max)
					{
						StackTop->Min = Current.Min;
						StackTop->Max = Inner.Max - 1;
						StackTop->MaxDepth = Current.MaxDepth;
						StackTop++;
					}
					if (Current.Max > Inner.Min)
					{
						Current.Min = Inner.Min;
						goto Loop;
					}
				}
				else
				{
					if (Current.Max > Inner.Min)
					{
						StackTop->Min = Inner.Min;
						StackTop->Max = Current.Max;
						StackTop->MaxDepth = Current.MaxDepth;
						StackTop++;
					}
					if (Current.Min + 1 < Inner.Max)
					{
						Current.Max = Inner.Max - 1;
						goto Loop;
					}
				}
			}
		}
	}
} // namespace AlgoImpl

namespace Algo
{
	/** Unstable in-place sort of a contiguous range (UE: Algo::IntroSort). */
	template <typename RangeType>
	FORCEINLINE void IntroSort(RangeType&& Range)
	{
		AlgoImpl::IntroSortInternal(GetData(Range), SIZE_T(GetNum(Range)), FIdentityFunctor(), TLess<>());
	}

	template <typename RangeType, typename PredicateType>
	FORCEINLINE void IntroSort(RangeType&& Range, PredicateType Predicate)
	{
		AlgoImpl::IntroSortInternal(GetData(Range), SIZE_T(GetNum(Range)), FIdentityFunctor(), MoveTemp(Predicate));
	}

	template <typename RangeType, typename ProjectionType>
	FORCEINLINE void IntroSortBy(RangeType&& Range, ProjectionType Projection)
	{
		AlgoImpl::IntroSortInternal(GetData(Range), SIZE_T(GetNum(Range)), MoveTemp(Projection), TLess<>());
	}

	template <typename RangeType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void IntroSortBy(RangeType&& Range, ProjectionType Projection, PredicateType Predicate)
	{
		AlgoImpl::IntroSortInternal(GetData(Range), SIZE_T(GetNum(Range)), MoveTemp(Projection), MoveTemp(Predicate));
	}
} // namespace Algo
