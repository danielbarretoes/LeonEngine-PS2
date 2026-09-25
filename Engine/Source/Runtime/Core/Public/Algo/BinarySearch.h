#pragma once

#include "CoreTypes.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Templates/UnrealTemplate.h"

namespace AlgoImpl
{
	/** First index whose projected value is not less than Value (UE). */
	template <typename RangeValueType, typename SizeType, typename PredicateValueType, typename ProjectionType,
		typename SortPredicateType>
	SizeType LowerBoundInternal(RangeValueType* First, const SizeType Num, const PredicateValueType& Value,
		ProjectionType Projection, SortPredicateType SortPredicate)
	{
		SizeType Start = 0;
		SizeType Size = Num;
		while (Size > 0)
		{
			const SizeType LeftoverSize = Size % 2;
			Size = Size / 2;

			const SizeType CheckIndex = Start + Size;
			const SizeType StartIfLess = CheckIndex + LeftoverSize;

			Start = SortPredicate(Invoke(Projection, First[CheckIndex]), Value) ? StartIfLess : Start;
		}
		return Start;
	}

	/** First index whose projected value is greater than Value (UE). */
	template <typename RangeValueType, typename SizeType, typename PredicateValueType, typename ProjectionType,
		typename SortPredicateType>
	SizeType UpperBoundInternal(RangeValueType* First, const SizeType Num, const PredicateValueType& Value,
		ProjectionType Projection, SortPredicateType SortPredicate)
	{
		SizeType Start = 0;
		SizeType Size = Num;
		while (Size > 0)
		{
			const SizeType LeftoverSize = Size % 2;
			Size = Size / 2;

			const SizeType CheckIndex = Start + Size;
			const SizeType StartIfLess = CheckIndex + LeftoverSize;

			Start = !SortPredicate(Value, Invoke(Projection, First[CheckIndex])) ? StartIfLess : Start;
		}
		return Start;
	}
} // namespace AlgoImpl

namespace Algo
{
	template <typename RangeType, typename ValueType, typename SortPredicateType>
	FORCEINLINE auto LowerBound(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
		-> decltype(GetNum(Range))
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), SortPredicate);
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE auto LowerBound(RangeType& Range, const ValueType& Value) -> decltype(GetNum(Range))
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), TLess<>());
	}

	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE auto LowerBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection,
		SortPredicateType SortPredicate) -> decltype(GetNum(Range))
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, Projection, SortPredicate);
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE auto LowerBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
		-> decltype(GetNum(Range))
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, Projection, TLess<>());
	}

	template <typename RangeType, typename ValueType, typename SortPredicateType>
	FORCEINLINE auto UpperBound(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
		-> decltype(GetNum(Range))
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), SortPredicate);
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE auto UpperBound(RangeType& Range, const ValueType& Value) -> decltype(GetNum(Range))
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), TLess<>());
	}

	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE auto UpperBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection,
		SortPredicateType SortPredicate) -> decltype(GetNum(Range))
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, Projection, SortPredicate);
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE auto UpperBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
		-> decltype(GetNum(Range))
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, Projection, TLess<>());
	}

	/** Index of an element equal to Value in a sorted range, or INDEX_NONE (UE: Algo::BinarySearch). */
	template <typename RangeType, typename ValueType, typename SortPredicateType>
	auto BinarySearch(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
		-> decltype(GetNum(Range))
	{
		auto CheckIndex = LowerBound(Range, Value, SortPredicate);
		if (CheckIndex < GetNum(Range))
		{
			auto&& CheckValue = GetData(Range)[CheckIndex];
			// Since we returned lower bound we already know Value <= CheckValue. So if Value is not < CheckValue, they
			// must be equal.
			if (!SortPredicate(Value, CheckValue))
			{
				return CheckIndex;
			}
		}
		return INDEX_NONE;
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE auto BinarySearch(RangeType& Range, const ValueType& Value) -> decltype(GetNum(Range))
	{
		return BinarySearch(Range, Value, TLess<>());
	}

	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	auto BinarySearchBy(RangeType& Range, const ValueType& Value, ProjectionType Projection,
		SortPredicateType SortPredicate) -> decltype(GetNum(Range))
	{
		auto CheckIndex = LowerBoundBy(Range, Value, Projection, SortPredicate);
		if (CheckIndex < GetNum(Range))
		{
			auto&& CheckValue = Invoke(Projection, GetData(Range)[CheckIndex]);
			if (!SortPredicate(Value, CheckValue))
			{
				return CheckIndex;
			}
		}
		return INDEX_NONE;
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE auto BinarySearchBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
		-> decltype(GetNum(Range))
	{
		return BinarySearchBy(Range, Value, Projection, TLess<>());
	}
} // namespace Algo
