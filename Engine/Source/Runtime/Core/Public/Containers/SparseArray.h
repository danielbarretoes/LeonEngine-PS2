#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/MemoryOps.h"
#include "Templates/Sorting.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"

#include <new>
#include <type_traits>

/** Result of TSparseArray::AddUninitialized: the slot index and its raw memory (UE). */
struct FSparseArrayAllocationInfo
{
	int32 Index;
	void* Pointer;
};

/** Placement new into a TSparseArray slot: new (AllocationInfo) FElement(...). */
inline void* operator new(size_t Size, const FSparseArrayAllocationInfo& Allocation)
{
	(void)Size;
	return Allocation.Pointer;
}
inline void operator delete(void*, const FSparseArrayAllocationInfo&)
{
}

/** An element slot: the element when allocated, a free-list link when free (UE: TSparseArrayElementOrFreeListLink). */
template <typename ElementType>
union TSparseArrayElementOrFreeListLink
{
	ElementType ElementData;
	struct
	{
		int32 PrevFreeIndex;
		int32 NextFreeIndex;
	} Link;
};

/**
 * Array with stable indices: removal leaves a hole that the next add reuses (UE: TSparseArray). Iteration visits the
 * allocated elements in index order.
 */
template <typename InElementType, typename Allocator /* = FDefaultSparseArrayAllocator */>
class TSparseArray
{
	using ElementType = InElementType;

	template <typename, typename>
	friend class TSparseArray;

public:
	typedef TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(ElementType), alignof(ElementType)>>
		FElementOrFreeListLink;

	TSparseArray()
		: FirstFreeIndex(-1)
		, NumFreeIndices(0)
	{
	}

	TSparseArray(TSparseArray&& InCopy)
		: FirstFreeIndex(-1)
		, NumFreeIndices(0)
	{
		MoveOrCopy(*this, InCopy);
	}

	TSparseArray(const TSparseArray& InCopy)
		: FirstFreeIndex(-1)
		, NumFreeIndices(0)
	{
		*this = InCopy;
	}

	~TSparseArray()
	{
		// Destruct the elements in the array.
		Empty();
	}

	TSparseArray& operator=(TSparseArray&& InCopy)
	{
		if (this != &InCopy)
		{
			MoveOrCopy(*this, InCopy);
		}
		return *this;
	}

	TSparseArray& operator=(const TSparseArray& InCopy)
	{
		if (this != &InCopy)
		{
			const int32 SrcMax = InCopy.GetMaxIndex();

			// Reallocate the array.
			Empty(SrcMax);
			Data.AddUninitialized(SrcMax);

			// Copy the other array's element allocation state.
			FirstFreeIndex = InCopy.FirstFreeIndex;
			NumFreeIndices = InCopy.NumFreeIndices;
			AllocationFlags = InCopy.AllocationFlags;

			if constexpr (!std::is_trivially_copy_constructible_v<ElementType>)
			{
				FElementOrFreeListLink* DestData = Data.GetData();
				const FElementOrFreeListLink* SrcData = InCopy.Data.GetData();
				for (int32 Index = 0; Index < SrcMax; Index++)
				{
					FElementOrFreeListLink& DestElement = DestData[Index];
					const FElementOrFreeListLink& SrcElement = SrcData[Index];
					if (InCopy.IsAllocated(Index))
					{
						::new ((uint8*)&DestElement.ElementData)
							ElementType(*(const ElementType*)&SrcElement.ElementData);
					}
					else
					{
						DestElement.Link.PrevFreeIndex = SrcElement.Link.PrevFreeIndex;
						DestElement.Link.NextFreeIndex = SrcElement.Link.NextFreeIndex;
					}
				}
			}
			else if (SrcMax)
			{
				FMemory::Memcpy(Data.GetData(), InCopy.Data.GetData(), sizeof(FElementOrFreeListLink) * SrcMax);
			}
		}
		return *this;
	}

	/** Reserves a slot (a free one if any) and returns its index + memory; construct the element with placement new. */
	FSparseArrayAllocationInfo AddUninitialized()
	{
		int32 Index;
		if (NumFreeIndices)
		{
			// Remove and use the first index from the list of free elements.
			Index = FirstFreeIndex;
			FElementOrFreeListLink& IndexData = GetData(FirstFreeIndex);
			FirstFreeIndex = IndexData.Link.NextFreeIndex;
			--NumFreeIndices;
			if (NumFreeIndices)
			{
				GetData(IndexData.Link.NextFreeIndex).Link.PrevFreeIndex = -1;
			}
		}
		else
		{
			// Add a new element.
			Index = Data.AddUninitialized(1);
			AllocationFlags.Add(false);
		}
		return AllocateIndex(Index);
	}

	int32 Add(const ElementType& Element)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new (Allocation) ElementType(Element);
		return Allocation.Index;
	}

	int32 Add(ElementType&& Element)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new (Allocation) ElementType(MoveTempIfPossible(Element));
		return Allocation.Index;
	}

	template <typename... ArgsType>
	FORCEINLINE int32 Emplace(ArgsType&&... Args)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new (Allocation) ElementType(Forward<ArgsType>(Args)...);
		return Allocation.Index;
	}

	/** Marks a free slot as allocated (the index must be on the free list). */
	FSparseArrayAllocationInfo AllocateIndex(int32 Index)
	{
		check(Index >= 0);
		check(Index < GetMaxIndex());
		check(!AllocationFlags[Index]);

		// Flag the element as allocated.
		AllocationFlags[Index] = true;

		FSparseArrayAllocationInfo Result;
		Result.Index = Index;
		Result.Pointer = &GetData(Result.Index).ElementData;
		return Result;
	}

	/** Destroys Count elements from Index and puts their slots on the free list. */
	void RemoveAt(int32 Index, int32 Count = 1)
	{
		if constexpr (!std::is_trivially_destructible_v<ElementType>)
		{
			for (int32 It = Index, ItCount = Count; ItCount; ++It, --ItCount)
			{
				((ElementType&)GetData(It).ElementData).~ElementType();
			}
		}
		RemoveAtUninitialized(Index, Count);
	}

	/** Frees Count slots from Index without destroying the elements. */
	void RemoveAtUninitialized(int32 Index, int32 Count = 1)
	{
		for (; Count; --Count)
		{
			check(AllocationFlags[Index]);

			// Mark the element as free and add it to the free element list.
			if (NumFreeIndices)
			{
				GetData(FirstFreeIndex).Link.PrevFreeIndex = Index;
			}
			FElementOrFreeListLink& IndexData = GetData(Index);
			IndexData.Link.PrevFreeIndex = -1;
			IndexData.Link.NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
			FirstFreeIndex = Index;
			++NumFreeIndices;
			AllocationFlags[Index] = false;

			++Index;
		}
	}

	/** Removes every element; the storage shrinks / grows to ExpectedNumElements. */
	void Empty(int32 ExpectedNumElements = 0)
	{
		if constexpr (!std::is_trivially_destructible_v<ElementType>)
		{
			for (TIterator It(*this); It; ++It)
			{
				ElementType& Element = *It;
				Element.~ElementType();
			}
		}

		Data.Empty(ExpectedNumElements);
		FirstFreeIndex = -1;
		NumFreeIndices = 0;
		AllocationFlags.Empty(ExpectedNumElements);
	}

	/** Removes every element, keeping the storage. */
	void Reset()
	{
		if constexpr (!std::is_trivially_destructible_v<ElementType>)
		{
			for (TIterator It(*this); It; ++It)
			{
				ElementType& Element = *It;
				Element.~ElementType();
			}
		}

		Data.Reset();
		FirstFreeIndex = -1;
		NumFreeIndices = 0;
		AllocationFlags.Reset();
	}

	/** Makes room for ExpectedNumElements slots, adding the new ones to the free list. */
	void Reserve(int32 ExpectedNumElements)
	{
		if (ExpectedNumElements > Data.Num())
		{
			const int32 ElementsToAdd = ExpectedNumElements - Data.Num();

			// Allocate memory in the array itself.
			const int32 ElementIndex = Data.AddUninitialized(ElementsToAdd);

			// Now mark the new elements as free.
			for (int32 FreeIndex = ExpectedNumElements - 1; FreeIndex >= ElementIndex; --FreeIndex)
			{
				if (NumFreeIndices)
				{
					GetData(FirstFreeIndex).Link.PrevFreeIndex = FreeIndex;
				}
				GetData(FreeIndex).Link.NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
				GetData(FreeIndex).Link.PrevFreeIndex = -1;
				FirstFreeIndex = FreeIndex;
				++NumFreeIndices;
			}

			if (ElementsToAdd == ExpectedNumElements)
			{
				AllocationFlags.Init(false, ElementsToAdd);
			}
			else
			{
				AllocationFlags.Add(false, ElementsToAdd);
			}
		}
	}

	/** Drops the unallocated slots at the end and releases the slack. */
	void Shrink()
	{
		// Determine the highest allocated index in the data array.
		const int32 MaxAllocatedIndex = AllocationFlags.FindLast(true);
		const int32 FirstIndexToRemove = MaxAllocatedIndex + 1;
		if (FirstIndexToRemove < Data.Num())
		{
			if (NumFreeIndices > 0)
			{
				// Look for elements in the free list that are in the memory to be freed.
				int32 FreeIndex = FirstFreeIndex;
				while (FreeIndex != INDEX_NONE)
				{
					if (FreeIndex >= FirstIndexToRemove)
					{
						const int32 PrevFreeIndex = GetData(FreeIndex).Link.PrevFreeIndex;
						const int32 NextFreeIndex = GetData(FreeIndex).Link.NextFreeIndex;
						if (NextFreeIndex != -1)
						{
							GetData(NextFreeIndex).Link.PrevFreeIndex = PrevFreeIndex;
						}
						if (PrevFreeIndex != -1)
						{
							GetData(PrevFreeIndex).Link.NextFreeIndex = NextFreeIndex;
						}
						else
						{
							FirstFreeIndex = NextFreeIndex;
						}
						--NumFreeIndices;

						FreeIndex = NextFreeIndex;
					}
					else
					{
						FreeIndex = GetData(FreeIndex).Link.NextFreeIndex;
					}
				}
			}

			// Truncate unallocated elements at the end of the data array.
			Data.RemoveAt(FirstIndexToRemove, Data.Num() - FirstIndexToRemove);
			AllocationFlags.RemoveAt(FirstIndexToRemove, AllocationFlags.Num() - FirstIndexToRemove);
		}

		// Shrink the data array.
		Data.Shrink();
	}

	/** Moves the last elements into the holes so every index < Num() is allocated; order not kept. */
	bool Compact()
	{
		const int32 NumFree = NumFreeIndices;
		if (NumFree == 0)
		{
			return false;
		}

		bool bResult = false;

		FElementOrFreeListLink* ElementData = Data.GetData();

		int32 EndIndex = Data.Num();
		const int32 TargetIndex = EndIndex - NumFree;
		int32 FreeIndex = FirstFreeIndex;
		while (FreeIndex != -1)
		{
			const int32 NextFreeIndex = GetData(FreeIndex).Link.NextFreeIndex;
			if (FreeIndex < TargetIndex)
			{
				// We need an element here.
				do
				{
					--EndIndex;
				} while (!AllocationFlags[EndIndex]);

				RelocateConstructItems<FElementOrFreeListLink>(ElementData + FreeIndex, ElementData + EndIndex, 1);
				AllocationFlags[FreeIndex] = true;

				bResult = true;
			}

			FreeIndex = NextFreeIndex;
		}

		Data.RemoveAt(TargetIndex, NumFree);
		AllocationFlags.RemoveAt(TargetIndex, NumFree);

		NumFreeIndices = 0;
		FirstFreeIndex = -1;

		return bResult;
	}

	/** Compact() keeping the elements' order. */
	bool CompactStable()
	{
		if (NumFreeIndices == 0)
		{
			return false;
		}

		// Copy the existing elements to a new array.
		TSparseArray<ElementType, Allocator> CompactedArray;
		CompactedArray.Empty(Num());
		for (TIterator It(*this); It; ++It)
		{
			new (CompactedArray.AddUninitialized()) ElementType(MoveTempIfPossible(*It));
		}

		// Replace this array with the compacted array.
		::Swap(*this, CompactedArray);

		return true;
	}

	/** Compacts and sorts the elements (unstable). */
	template <class PredicateClass>
	void Sort(const PredicateClass& Predicate)
	{
		if (Num() > 0)
		{
			// Compact the elements array so all the elements are contiguous.
			Compact();

			// Sort the elements according to the provided comparison class.
			::Sort(&GetData(0), Num(), FElementCompareClass<PredicateClass>(Predicate));
		}
	}

	void Sort()
	{
		Sort(TLess<ElementType>());
	}

	template <class PredicateClass>
	void StableSort(const PredicateClass& Predicate)
	{
		if (Num() > 0)
		{
			CompactStable();
			::StableSort(&GetData(0), Num(), FElementCompareClass<PredicateClass>(Predicate));
		}
	}

	void StableSort()
	{
		StableSort(TLess<ElementType>());
	}

	SIZE_T GetAllocatedSize() const
	{
		return (Data.Num() + Data.GetSlack()) * sizeof(FElementOrFreeListLink) + AllocationFlags.GetAllocatedSize();
	}

	bool operator==(const TSparseArray& B) const
	{
		if (GetMaxIndex() != B.GetMaxIndex())
		{
			return false;
		}
		for (int32 ElementIndex = 0; ElementIndex < GetMaxIndex(); ElementIndex++)
		{
			const bool bIsAllocatedA = IsAllocated(ElementIndex);
			const bool bIsAllocatedB = B.IsAllocated(ElementIndex);
			if (bIsAllocatedA != bIsAllocatedB)
			{
				return false;
			}
			if (bIsAllocatedA && !((*this)[ElementIndex] == B[ElementIndex]))
			{
				return false;
			}
		}
		return true;
	}

	bool operator!=(const TSparseArray& B) const
	{
		return !(*this == B);
	}

	/** True when every index < Num() is allocated. */
	bool IsCompact() const
	{
		return NumFreeIndices == 0;
	}

	FORCEINLINE bool IsAllocated(int32 Index) const
	{
		return AllocationFlags[Index];
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Data.Num();
	}

	FORCEINLINE int32 Num() const
	{
		return Data.Num() - NumFreeIndices;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Num() == 0;
	}

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return AllocationFlags.IsValidIndex(Index) && AllocationFlags[Index];
	}

	FORCEINLINE ElementType& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < Data.Num() && Index < AllocationFlags.Num());
		return *(ElementType*)&GetData(Index).ElementData;
	}

	FORCEINLINE const ElementType& operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < Data.Num() && Index < AllocationFlags.Num());
		return *(const ElementType*)&GetData(Index).ElementData;
	}

	/** Index of an element of this array from its address. */
	int32 PointerToIndex(const ElementType* Ptr) const
	{
		checkSlow(Data.Num());
		const int32 Index = int32((const FElementOrFreeListLink*)Ptr - Data.GetData());
		checkSlow(Index >= 0 && Index < Data.Num() && Index < AllocationFlags.Num() && AllocationFlags[Index]);
		return Index;
	}

private:
	template <bool bConst>
	class TBaseIterator
	{
	public:
		typedef TConstSetBitIterator<typename Allocator::BitArrayAllocator> BitArrayItType;

	private:
		typedef std::conditional_t<bConst, const TSparseArray, TSparseArray> ArrayType;
		typedef std::conditional_t<bConst, const ElementType, ElementType> ItElementType;

	public:
		explicit TBaseIterator(ArrayType& InArray, const BitArrayItType& InBitArrayIt)
			: Array(InArray)
			, BitArrayIt(InBitArrayIt)
		{
		}

		FORCEINLINE TBaseIterator& operator++()
		{
			// Iterate to the next set allocation flag.
			++BitArrayIt;
			return *this;
		}

		FORCEINLINE int32 GetIndex() const
		{
			return BitArrayIt.GetIndex();
		}

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs)
		{
			return Lhs.BitArrayIt == Rhs.BitArrayIt && &Lhs.Array == &Rhs.Array;
		}
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs)
		{
			return !(Lhs == Rhs);
		}

		FORCEINLINE explicit operator bool() const
		{
			return !!BitArrayIt;
		}

		FORCEINLINE ItElementType& operator*() const
		{
			return Array[GetIndex()];
		}
		FORCEINLINE ItElementType* operator->() const
		{
			return &Array[GetIndex()];
		}

		FORCEINLINE const FRelativeBitReference GetRelativeBitReference() const
		{
			return FRelativeBitReference(BitArrayIt.GetIndex());
		}

	protected:
		ArrayType& Array;
		BitArrayItType BitArrayIt;
	};

public:
	/** Iterator that can remove the current element (UE: TSparseArray::TIterator). */
	class TIterator : public TBaseIterator<false>
	{
	public:
		TIterator(TSparseArray& InArray)
			: TBaseIterator<false>(
				  InArray, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(InArray.AllocationFlags))
		{
		}

		TIterator(TSparseArray& InArray, const typename TBaseIterator<false>::BitArrayItType& InBitArrayIt)
			: TBaseIterator<false>(InArray, InBitArrayIt)
		{
		}

		/** Safe: the next element's slot is not affected by removing the current one. */
		void RemoveCurrent()
		{
			this->Array.RemoveAt(this->GetIndex());
		}
	};

	class TConstIterator : public TBaseIterator<true>
	{
	public:
		TConstIterator(const TSparseArray& InArray)
			: TBaseIterator<true>(
				  InArray, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(InArray.AllocationFlags))
		{
		}

		TConstIterator(const TSparseArray& InArray, const typename TBaseIterator<true>::BitArrayItType& InBitArrayIt)
			: TBaseIterator<true>(InArray, InBitArrayIt)
		{
		}
	};

	TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	// Ranged-for support (lower-case names required by the language).
	FORCEINLINE TIterator begin()
	{
		return TIterator(*this, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(AllocationFlags));
	}
	FORCEINLINE TConstIterator begin() const
	{
		return TConstIterator(*this, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(AllocationFlags));
	}
	FORCEINLINE TIterator end()
	{
		return TIterator(
			*this, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(AllocationFlags, AllocationFlags.Num()));
	}
	FORCEINLINE TConstIterator end() const
	{
		return TConstIterator(
			*this, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(AllocationFlags, AllocationFlags.Num()));
	}

private:
	/** Compares elements through their slots. */
	template <typename PredicateClass>
	class FElementCompareClass
	{
		const PredicateClass& Predicate;

	public:
		FElementCompareClass(const PredicateClass& InPredicate)
			: Predicate(InPredicate)
		{
		}

		bool operator()(const FElementOrFreeListLink& A, const FElementOrFreeListLink& B) const
		{
			return Predicate(*(const ElementType*)&A.ElementData, *(const ElementType*)&B.ElementData);
		}
	};

	template <typename SparseArrayType>
	static FORCEINLINE void MoveOrCopy(SparseArrayType& ToArray, SparseArrayType& FromArray)
	{
		ToArray.Empty();
		ToArray.Data = MoveTemp(FromArray.Data);
		ToArray.AllocationFlags = MoveTemp(FromArray.AllocationFlags);

		ToArray.FirstFreeIndex = FromArray.FirstFreeIndex;
		ToArray.NumFreeIndices = FromArray.NumFreeIndices;
		FromArray.FirstFreeIndex = -1;
		FromArray.NumFreeIndices = 0;
	}

	FORCEINLINE FElementOrFreeListLink& GetData(int32 Index)
	{
		return Data.GetData()[Index];
	}
	FORCEINLINE const FElementOrFreeListLink& GetData(int32 Index) const
	{
		return Data.GetData()[Index];
	}

	typedef TArray<FElementOrFreeListLink, typename Allocator::ElementAllocator> DataType;
	DataType Data;

	typedef TBitArray<typename Allocator::BitArrayAllocator> AllocationBitArrayType;
	AllocationBitArrayType AllocationFlags;

	/** Head of the doubly linked free list (INDEX_NONE when empty). */
	int32 FirstFreeIndex;

	/** Number of slots on the free list. */
	int32 NumFreeIndices;
};

template <typename ElementType, typename Allocator>
struct TIsZeroConstructType<TSparseArray<ElementType, Allocator>>
{
	enum
	{
		Value = false
	};
};
