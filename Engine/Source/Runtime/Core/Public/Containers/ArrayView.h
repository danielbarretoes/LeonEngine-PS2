#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/Sorting.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <initializer_list>
#include <type_traits>

/**
 * Non-owning view of contiguous elements (UE: TArrayView). TConstArrayView<T> views const elements. The viewed
 * memory must outlive the view.
 */
template <typename InElementType>
class TArrayView
{
public:
	using ElementType = InElementType;
	using SizeType = int32;

	TArrayView()
		: DataPtr(nullptr)
		, ArrayNum(0)
	{
	}

	/** View of any contiguous container with GetData / Num (TArray, C array, TArrayView...). */
	template <typename OtherRangeType,
		typename CVUnqualifiedOtherRangeType = std::remove_cv_t<std::remove_reference_t<OtherRangeType>>,
		std::enable_if_t<TIsContiguousContainer<CVUnqualifiedOtherRangeType>::Value &&
				std::is_convertible_v<std::remove_pointer_t<decltype(::GetData(std::declval<OtherRangeType&>()))> (*)[],
					ElementType (*)[]>,
			int> = 0>
	FORCEINLINE TArrayView(OtherRangeType&& Other)
		: DataPtr(::GetData(Other))
		, ArrayNum(SizeType(::GetNum(Other)))
	{
	}

	template <typename OtherElementType,
		std::enable_if_t<std::is_convertible_v<OtherElementType (*)[], ElementType (*)[]>, int> = 0>
	FORCEINLINE TArrayView(OtherElementType* InData, SizeType InCount)
		: DataPtr(InData)
		, ArrayNum(InCount)
	{
		check(ArrayNum >= 0);
	}

	/** View of an initializer list; only valid for the full expression (use with const element types). */
	FORCEINLINE TArrayView(std::initializer_list<std::remove_const_t<ElementType>> List)
		: DataPtr(List.begin())
		, ArrayNum(SizeType(List.size()))
	{
	}

	FORCEINLINE ElementType* GetData() const
	{
		return DataPtr;
	}

	static constexpr SIZE_T GetTypeSize()
	{
		return sizeof(ElementType);
	}

	FORCEINLINE void CheckInvariants() const
	{
		checkSlow(ArrayNum >= 0);
	}

	FORCEINLINE void RangeCheck(SizeType Index) const
	{
		CheckInvariants();
		checkf((Index >= 0) & (Index < ArrayNum), "Array index out of bounds: %d from an array of size %d", int(Index),
			int(ArrayNum));
	}

	FORCEINLINE void SliceRangeCheck(SizeType Index, SizeType InNum) const
	{
		checkf(Index >= 0, "Invalid index (%d)", int(Index));
		checkf(InNum >= 0, "Invalid count (%d)", int(InNum));
		checkf(Index + InNum <= ArrayNum, "Range (index: %d, count: %d) lies outside the view of %d elements",
			int(Index), int(InNum), int(ArrayNum));
	}

	FORCEINLINE bool IsValidIndex(SizeType Index) const
	{
		return (Index >= 0) && (Index < ArrayNum);
	}

	bool IsEmpty() const
	{
		return ArrayNum == 0;
	}

	FORCEINLINE SizeType Num() const
	{
		return ArrayNum;
	}

	FORCEINLINE ElementType& operator[](SizeType Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	FORCEINLINE ElementType& Last(SizeType IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	FORCEINLINE TArrayView Slice(SizeType Index, SizeType InNum) const
	{
		SliceRangeCheck(Index, InNum);
		return TArrayView(DataPtr + Index, InNum);
	}

	FORCEINLINE TArrayView Left(SizeType Count) const
	{
		return TArrayView(DataPtr, FMath::Clamp(Count, 0, ArrayNum));
	}

	FORCEINLINE TArrayView LeftChop(SizeType Count) const
	{
		return TArrayView(DataPtr, FMath::Clamp(ArrayNum - Count, 0, ArrayNum));
	}

	FORCEINLINE TArrayView Right(SizeType Count) const
	{
		const SizeType OutputArrayNum = FMath::Clamp(Count, 0, ArrayNum);
		return TArrayView(DataPtr + ArrayNum - OutputArrayNum, OutputArrayNum);
	}

	FORCEINLINE TArrayView RightChop(SizeType Count) const
	{
		const SizeType OutputArrayNum = FMath::Clamp(ArrayNum - Count, 0, ArrayNum);
		return TArrayView(DataPtr + ArrayNum - OutputArrayNum, OutputArrayNum);
	}

	FORCEINLINE TArrayView Mid(SizeType Index, SizeType Count = TNumericLimitsInt32Max) const
	{
		ElementType* CurrentStart = GetData();
		const SizeType CurrentNum = Num();

		// Clamp the start index to [0, Num].
		Index = FMath::Clamp(Index, 0, CurrentNum);
		// Clamp the end to [Index, Num].
		const SizeType End = (Count > CurrentNum - Index) ? CurrentNum : Index + FMath::Max(Count, 0);
		return TArrayView(CurrentStart + Index, End - Index);
	}

	SizeType Find(const ElementType& Item) const
	{
		const ElementType* Data = GetData();
		for (SizeType Index = 0; Index < ArrayNum; ++Index)
		{
			if (Data[Index] == Item)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FORCEINLINE bool Find(const ElementType& Item, SizeType& Index) const
	{
		Index = Find(Item);
		return Index != INDEX_NONE;
	}

	SizeType FindLast(const ElementType& Item) const
	{
		const ElementType* Data = GetData();
		for (SizeType Index = ArrayNum - 1; Index >= 0; --Index)
		{
			if (Data[Index] == Item)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	template <typename KeyType>
	SizeType IndexOfByKey(const KeyType& Key) const
	{
		const ElementType* Data = GetData();
		for (SizeType Index = 0; Index < ArrayNum; ++Index)
		{
			if (Data[Index] == Key)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	template <typename Predicate>
	SizeType IndexOfByPredicate(Predicate Pred) const
	{
		const ElementType* Data = GetData();
		for (SizeType Index = 0; Index < ArrayNum; ++Index)
		{
			if (::Invoke(Pred, Data[Index]))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	template <typename KeyType>
	ElementType* FindByKey(const KeyType& Key) const
	{
		const SizeType Index = IndexOfByKey(Key);
		return Index != INDEX_NONE ? GetData() + Index : nullptr;
	}

	template <typename Predicate>
	ElementType* FindByPredicate(Predicate Pred) const
	{
		const SizeType Index = IndexOfByPredicate(Pred);
		return Index != INDEX_NONE ? GetData() + Index : nullptr;
	}

	template <typename ComparisonType>
	FORCEINLINE bool Contains(const ComparisonType& Item) const
	{
		return IndexOfByKey(Item) != INDEX_NONE;
	}

	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return IndexOfByPredicate(Pred) != INDEX_NONE;
	}

	void Sort()
	{
		::Sort(GetData(), Num());
	}
	template <class PredicateType>
	void Sort(const PredicateType& Predicate)
	{
		::Sort(GetData(), Num(), Predicate);
	}
	void StableSort()
	{
		::StableSort(GetData(), Num());
	}
	template <class PredicateType>
	void StableSort(const PredicateType& Predicate)
	{
		::StableSort(GetData(), Num(), Predicate);
	}

	// Ranged-for support (lower-case names required by the language).
	FORCEINLINE ElementType* begin() const
	{
		return GetData();
	}
	FORCEINLINE ElementType* end() const
	{
		return GetData() + Num();
	}

private:
	static constexpr SizeType TNumericLimitsInt32Max = 0x7fffffff;

	ElementType* DataPtr;
	SizeType ArrayNum;
};

template <typename InElementType>
struct TIsContiguousContainer<TArrayView<InElementType>>
{
	enum
	{
		Value = true
	};
};

template <typename ElementType>
FORCEINLINE TArrayView<ElementType> MakeArrayView(ElementType* Pointer, int32 Size)
{
	return TArrayView<ElementType>(Pointer, Size);
}

template <typename ElementType, typename Allocator>
FORCEINLINE TArrayView<ElementType> MakeArrayView(TArray<ElementType, Allocator>& Other)
{
	return TArrayView<ElementType>(Other);
}

template <typename ElementType, typename Allocator>
FORCEINLINE TArrayView<const ElementType> MakeArrayView(const TArray<ElementType, Allocator>& Other)
{
	return TArrayView<const ElementType>(Other);
}

template <typename ElementType, SIZE_T N>
FORCEINLINE TArrayView<ElementType> MakeArrayView(ElementType (&Other)[N])
{
	return TArrayView<ElementType>(Other);
}

template <typename ElementType>
FORCEINLINE TArrayView<const ElementType> MakeArrayView(std::initializer_list<ElementType> List)
{
	return TArrayView<const ElementType>(List.begin(), int32(List.size()));
}
