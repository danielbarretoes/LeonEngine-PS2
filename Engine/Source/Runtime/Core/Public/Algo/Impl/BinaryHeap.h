#pragma once

#include "CoreTypes.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTemplate.h"

// Binary heap primitives over a raw range (UE: Algo/Impl/BinaryHeap.h). With TLess the smallest element is at
// index 0 (a min-heap), like UE's TArray::Heap* functions.

namespace AlgoImpl
{
	FORCEINLINE int32 HeapGetLeftChildIndex(int32 Index)
	{
		return Index * 2 + 1;
	}

	FORCEINLINE bool HeapIsLeaf(int32 Index, int32 Count)
	{
		return HeapGetLeftChildIndex(Index) >= Count;
	}

	FORCEINLINE int32 HeapGetParentIndex(int32 Index)
	{
		return (Index - 1) / 2;
	}

	/** Moves the element at Index down until the heap property holds. */
	template <typename RangeValueType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void HeapSiftDown(RangeValueType* Heap, int32 Index, const int32 Count,
		const ProjectionType& Projection, const PredicateType& Predicate)
	{
		while (!HeapIsLeaf(Index, Count))
		{
			const int32 LeftChildIndex = HeapGetLeftChildIndex(Index);
			const int32 RightChildIndex = LeftChildIndex + 1;

			int32 MinChildIndex = LeftChildIndex;
			if (RightChildIndex < Count)
			{
				MinChildIndex =
					Predicate(Invoke(Projection, Heap[LeftChildIndex]), Invoke(Projection, Heap[RightChildIndex]))
					? LeftChildIndex
					: RightChildIndex;
			}

			if (!Predicate(Invoke(Projection, Heap[MinChildIndex]), Invoke(Projection, Heap[Index])))
			{
				break;
			}

			Swap(Heap[Index], Heap[MinChildIndex]);
			Index = MinChildIndex;
		}
	}

	/** Moves the element at NodeIndex up until the heap property holds; returns its final index. */
	template <typename RangeValueType, typename ProjectionType, typename PredicateType>
	FORCEINLINE int32 HeapSiftUp(RangeValueType* Heap, int32 RootIndex, int32 NodeIndex,
		const ProjectionType& Projection, const PredicateType& Predicate)
	{
		while (NodeIndex > RootIndex)
		{
			const int32 ParentIndex = HeapGetParentIndex(NodeIndex);
			if (!Predicate(Invoke(Projection, Heap[NodeIndex]), Invoke(Projection, Heap[ParentIndex])))
			{
				break;
			}
			Swap(Heap[NodeIndex], Heap[ParentIndex]);
			NodeIndex = ParentIndex;
		}
		return NodeIndex;
	}

	template <typename RangeValueType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void HeapifyInternal(
		RangeValueType* First, int32 Num, const ProjectionType& Projection, const PredicateType& Predicate)
	{
		for (int32 Index = HeapGetParentIndex(Num - 1); Index >= 0; Index--)
		{
			HeapSiftDown(First, Index, Num, Projection, Predicate);
		}
	}

	/** In-place heap sort; the reversed predicate builds the heap so the result is ascending. */
	template <typename RangeValueType, typename ProjectionType, typename PredicateType>
	void HeapSortInternal(RangeValueType* First, int32 Num, ProjectionType Projection, PredicateType Predicate)
	{
		auto ReversePredicate = [&Predicate](const auto& A, const auto& B) { return Predicate(B, A); };
		HeapifyInternal(First, Num, Projection, ReversePredicate);

		for (int32 Index = Num - 1; Index > 0; Index--)
		{
			Swap(First[0], First[Index]);
			HeapSiftDown(First, 0, Index, Projection, ReversePredicate);
		}
	}
} // namespace AlgoImpl
