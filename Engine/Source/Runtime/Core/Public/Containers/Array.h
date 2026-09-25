#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Templates/MemoryOps.h"
#include "Templates/Sorting.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <initializer_list>
#include <new>
#include <type_traits>

/** Index-based iterator over an indexed container; RemoveCurrent keeps iteration valid (UE). */
template <typename ContainerType, typename ElementType, typename SizeType>
class TIndexedContainerIterator
{
public:
	TIndexedContainerIterator(ContainerType& InContainer, SizeType StartIndex = 0)
		: Container(InContainer)
		, Index(StartIndex)
	{
	}

	TIndexedContainerIterator& operator++()
	{
		++Index;
		return *this;
	}
	TIndexedContainerIterator operator++(int)
	{
		TIndexedContainerIterator Tmp(*this);
		++Index;
		return Tmp;
	}
	TIndexedContainerIterator& operator--()
	{
		--Index;
		return *this;
	}
	TIndexedContainerIterator operator--(int)
	{
		TIndexedContainerIterator Tmp(*this);
		--Index;
		return Tmp;
	}
	TIndexedContainerIterator& operator+=(SizeType Offset)
	{
		Index += Offset;
		return *this;
	}
	TIndexedContainerIterator operator+(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp += Offset;
	}
	TIndexedContainerIterator& operator-=(SizeType Offset)
	{
		return *this += -Offset;
	}
	TIndexedContainerIterator operator-(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp -= Offset;
	}

	ElementType& operator*() const
	{
		return Container[Index];
	}
	ElementType* operator->() const
	{
		return &Container[Index];
	}

	FORCEINLINE explicit operator bool() const
	{
		return Container.IsValidIndex(Index);
	}

	SizeType GetIndex() const
	{
		return Index;
	}

	void Reset()
	{
		Index = 0;
	}

	void SetToEnd()
	{
		Index = Container.Num();
	}

	/** Removes the current element; the next ++ visits the element that followed it. */
	void RemoveCurrent()
	{
		Container.RemoveAt(Index);
		Index--;
	}

	FORCEINLINE bool operator==(const TIndexedContainerIterator& Rhs) const
	{
		return &Container == &Rhs.Container && Index == Rhs.Index;
	}
	FORCEINLINE bool operator!=(const TIndexedContainerIterator& Rhs) const
	{
		return !(*this == Rhs);
	}

private:
	ContainerType& Container;
	SizeType Index;
};

/** Ranged-for iterator that fails a check when the array changes size during the loop (UE: TCheckedPointerIterator). */
template <typename ElementType, typename SizeType>
struct TCheckedPointerIterator
{
	explicit TCheckedPointerIterator(const SizeType& InNum, ElementType* InPtr)
		: Ptr(InPtr)
		, CurrentNum(InNum)
		, InitialNum(InNum)
	{
	}

	FORCEINLINE ElementType& operator*() const
	{
		return *Ptr;
	}

	FORCEINLINE TCheckedPointerIterator& operator++()
	{
		++Ptr;
		return *this;
	}

	FORCEINLINE TCheckedPointerIterator& operator--()
	{
		--Ptr;
		return *this;
	}

	FORCEINLINE bool operator!=(const TCheckedPointerIterator& Rhs) const
	{
		checkf(CurrentNum == InitialNum, "Array has changed during ranged-for iteration!");
		return Ptr != Rhs.Ptr;
	}

private:
	ElementType* Ptr;
	const SizeType& CurrentNum;
	SizeType InitialNum;
};

/**
 * Dynamic array of contiguous elements (UE: TArray). Elements are relocated with memmove when the array grows, so
 * element types must not point into themselves (the UE rule).
 */
template <typename InElementType, typename InAllocatorType>
class TArray
{
	template <typename OtherInElementType, typename OtherAllocator>
	friend class TArray;

public:
	typedef typename InAllocatorType::SizeType SizeType;
	typedef InElementType ElementType;
	typedef InAllocatorType AllocatorType;

	typedef std::conditional_t<AllocatorType::NeedsElementType,
		typename AllocatorType::template ForElementType<ElementType>, typename AllocatorType::ForAnyElementType>
		ElementAllocatorType;

	static_assert(std::is_signed_v<SizeType>, "TArray only supports signed index types");

	FORCEINLINE TArray()
		: ArrayNum(0)
		, ArrayMax(AllocatorInstance.GetInitialCapacity())
	{
	}

	FORCEINLINE TArray(const ElementType* Ptr, SizeType Count)
	{
		check(Ptr != nullptr || Count == 0);
		CopyToEmpty(Ptr, Count, 0, 0);
	}

	TArray(std::initializer_list<InElementType> InitList)
	{
		CopyToEmpty(InitList.begin(), SizeType(InitList.size()), 0, 0);
	}

	template <typename OtherElementType, typename OtherAllocator>
	FORCEINLINE explicit TArray(const TArray<OtherElementType, OtherAllocator>& Other)
	{
		CopyToEmpty(Other.GetData(), Other.Num(), 0, 0);
	}

	FORCEINLINE TArray(const TArray& Other)
	{
		CopyToEmpty(Other.GetData(), Other.Num(), 0, 0);
	}

	FORCEINLINE TArray(const TArray& Other, SizeType ExtraSlack)
	{
		CopyToEmpty(Other.GetData(), Other.Num(), 0, ExtraSlack);
	}

	FORCEINLINE TArray(TArray&& Other)
	{
		MoveOrCopy(*this, Other, 0);
	}

	template <typename OtherElementType, typename OtherAllocator>
	FORCEINLINE explicit TArray(TArray<OtherElementType, OtherAllocator>&& Other)
	{
		MoveOrCopy(*this, Other, 0);
	}

	TArray& operator=(std::initializer_list<InElementType> InitList)
	{
		DestructItems(GetData(), ArrayNum);
		CopyToEmpty(InitList.begin(), SizeType(InitList.size()), ArrayMax, 0);
		return *this;
	}

	template <typename OtherAllocatorType>
	TArray& operator=(const TArray<ElementType, OtherAllocatorType>& Other)
	{
		DestructItems(GetData(), ArrayNum);
		CopyToEmpty(Other.GetData(), Other.Num(), ArrayMax, 0);
		return *this;
	}

	TArray& operator=(const TArray& Other)
	{
		if (this != &Other)
		{
			DestructItems(GetData(), ArrayNum);
			CopyToEmpty(Other.GetData(), Other.Num(), ArrayMax, 0);
		}
		return *this;
	}

	TArray& operator=(TArray&& Other)
	{
		if (this != &Other)
		{
			DestructItems(GetData(), ArrayNum);
			MoveOrCopy(*this, Other, ArrayMax);
		}
		return *this;
	}

	~TArray()
	{
		DestructItems(GetData(), ArrayNum);
	}

	FORCEINLINE ElementType* GetData()
	{
		return static_cast<ElementType*>(AllocatorInstance.GetAllocation());
	}
	FORCEINLINE const ElementType* GetData() const
	{
		return static_cast<const ElementType*>(AllocatorInstance.GetAllocation());
	}

	FORCEINLINE uint32 GetTypeSize() const
	{
		return sizeof(ElementType);
	}

	FORCEINLINE SIZE_T GetAllocatedSize() const
	{
		return AllocatorInstance.GetAllocatedSize(ArrayMax, sizeof(ElementType));
	}

	FORCEINLINE SizeType GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}

	FORCEINLINE void CheckInvariants() const
	{
		checkSlow((ArrayNum >= 0) & (ArrayMax >= ArrayNum));
	}

	FORCEINLINE void RangeCheck(SizeType Index) const
	{
		CheckInvariants();
		if constexpr (AllocatorType::RequireRangeCheck)
		{
			checkf((Index >= 0) & (Index < ArrayNum), "Array index out of bounds: %d from an array of size %d",
				int(Index), int(ArrayNum));
		}
	}

	FORCEINLINE bool IsValidIndex(SizeType Index) const
	{
		return Index >= 0 && Index < ArrayNum;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return ArrayNum == 0;
	}

	FORCEINLINE SizeType Num() const
	{
		return ArrayNum;
	}

	FORCEINLINE SizeType Max() const
	{
		return ArrayMax;
	}

	FORCEINLINE ElementType& operator[](SizeType Index)
	{
		RangeCheck(Index);
		return GetData()[Index];
	}
	FORCEINLINE const ElementType& operator[](SizeType Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/** Removes and returns the last element. */
	FORCEINLINE ElementType Pop(bool bAllowShrinking = true)
	{
		RangeCheck(0);
		ElementType Result = MoveTempIfPossible(GetData()[ArrayNum - 1]);
		RemoveAt(ArrayNum - 1, 1, bAllowShrinking);
		return Result;
	}

	FORCEINLINE void Push(ElementType&& Item)
	{
		Add(MoveTempIfPossible(Item));
	}
	FORCEINLINE void Push(const ElementType& Item)
	{
		Add(Item);
	}

	FORCEINLINE ElementType& Top()
	{
		return Last();
	}
	FORCEINLINE const ElementType& Top() const
	{
		return Last();
	}

	FORCEINLINE ElementType& Last(SizeType IndexFromTheEnd = 0)
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}
	FORCEINLINE const ElementType& Last(SizeType IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/** Releases the slack. */
	FORCEINLINE void Shrink()
	{
		CheckInvariants();
		if (ArrayMax != ArrayNum)
		{
			ResizeTo(ArrayNum);
		}
	}

	// Searches -------------------------------------------------------------------------------------------------------

	FORCEINLINE bool Find(const ElementType& Item, SizeType& Index) const
	{
		Index = this->Find(Item);
		return Index != INDEX_NONE;
	}

	SizeType Find(const ElementType& Item) const
	{
		const ElementType* Start = GetData();
		for (const ElementType *Data = Start, *DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	FORCEINLINE bool FindLast(const ElementType& Item, SizeType& Index) const
	{
		Index = this->FindLast(Item);
		return Index != INDEX_NONE;
	}

	SizeType FindLast(const ElementType& Item) const
	{
		for (const ElementType *Start = GetData(), *Data = Start + ArrayNum; Data != Start;)
		{
			--Data;
			if (*Data == Item)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	template <typename Predicate>
	SizeType FindLastByPredicate(Predicate Pred, SizeType Count) const
	{
		check(Count >= 0 && Count <= this->Num());
		for (const ElementType *Start = GetData(), *Data = Start + Count; Data != Start;)
		{
			--Data;
			if (::Invoke(Pred, *Data))
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	template <typename Predicate>
	FORCEINLINE SizeType FindLastByPredicate(Predicate Pred) const
	{
		return FindLastByPredicate(Pred, ArrayNum);
	}

	template <typename KeyType>
	SizeType IndexOfByKey(const KeyType& Key) const
	{
		const ElementType* Start = GetData();
		for (const ElementType *Data = Start, *DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	template <typename Predicate>
	SizeType IndexOfByPredicate(Predicate Pred) const
	{
		const ElementType* Start = GetData();
		for (const ElementType *Data = Start, *DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	template <typename KeyType>
	FORCEINLINE const ElementType* FindByKey(const KeyType& Key) const
	{
		return const_cast<TArray*>(this)->FindByKey(Key);
	}

	template <typename KeyType>
	ElementType* FindByKey(const KeyType& Key)
	{
		for (ElementType *Data = GetData(), *DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return Data;
			}
		}
		return nullptr;
	}

	template <typename Predicate>
	FORCEINLINE const ElementType* FindByPredicate(Predicate Pred) const
	{
		return const_cast<TArray*>(this)->FindByPredicate(Pred);
	}

	template <typename Predicate>
	ElementType* FindByPredicate(Predicate Pred)
	{
		for (ElementType *Data = GetData(), *DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				return Data;
			}
		}
		return nullptr;
	}

	template <typename Predicate>
	TArray<std::remove_const_t<ElementType>> FilterByPredicate(Predicate Pred) const
	{
		TArray<std::remove_const_t<ElementType>> FilterResults;
		for (const ElementType *Data = GetData(), *DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				FilterResults.Add(*Data);
			}
		}
		return FilterResults;
	}

	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType *Data = GetData(), *DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return FindByPredicate(Pred) != nullptr;
	}

	bool operator==(const TArray& OtherArray) const
	{
		const SizeType Count = Num();
		return Count == OtherArray.Num() && CompareItems(GetData(), OtherArray.GetData(), Count);
	}

	FORCEINLINE bool operator!=(const TArray& OtherArray) const
	{
		return !(*this == OtherArray);
	}

	// Insertion ------------------------------------------------------------------------------------------------------

	/** Adds Count uninitialised elements at the end; returns the index of the first one. */
	FORCEINLINE SizeType AddUninitialized(SizeType Count = 1)
	{
		CheckInvariants();
		checkSlow(Count >= 0);

		const SizeType OldNum = ArrayNum;
		if ((ArrayNum += Count) > ArrayMax)
		{
			ResizeGrow(OldNum);
		}
		return OldNum;
	}

	void InsertUninitialized(SizeType Index, SizeType Count = 1)
	{
		CheckInvariants();
		checkSlow((Count >= 0) & (Index >= 0) & (Index <= ArrayNum));

		const SizeType OldNum = ArrayNum;
		if ((ArrayNum += Count) > ArrayMax)
		{
			ResizeGrow(OldNum);
		}
		ElementType* Data = GetData() + Index;
		RelocateConstructItems<ElementType>(Data + Count, Data, OldNum - Index);
	}

	void InsertZeroed(SizeType Index, SizeType Count = 1)
	{
		InsertUninitialized(Index, Count);
		FMemory::Memzero(GetData() + Index, Count * sizeof(ElementType));
	}

	ElementType& InsertZeroed_GetRef(SizeType Index) // NOLINT(readability-identifier-naming): UE name
	{
		InsertUninitialized(Index, 1);
		ElementType* Ptr = GetData() + Index;
		FMemory::Memzero(Ptr, sizeof(ElementType));
		return *Ptr;
	}

	void InsertDefaulted(SizeType Index, SizeType Count = 1)
	{
		InsertUninitialized(Index, Count);
		DefaultConstructItems<ElementType>(GetData() + Index, Count);
	}

	ElementType& InsertDefaulted_GetRef(SizeType Index) // NOLINT(readability-identifier-naming): UE name
	{
		InsertUninitialized(Index, 1);
		ElementType* Ptr = GetData() + Index;
		DefaultConstructItems<ElementType>(Ptr, 1);
		return *Ptr;
	}

	SizeType Insert(std::initializer_list<ElementType> InitList, const SizeType InIndex)
	{
		const SizeType NumNewElements = SizeType(InitList.size());
		InsertUninitialized(InIndex, NumNewElements);
		ConstructItems<ElementType>(GetData() + InIndex, InitList.begin(), NumNewElements);
		return InIndex;
	}

	template <typename OtherAllocator>
	SizeType Insert(const TArray<ElementType, OtherAllocator>& Items, const SizeType InIndex)
	{
		check((const void*)this != (const void*)&Items);
		const SizeType NumNewElements = Items.Num();
		InsertUninitialized(InIndex, NumNewElements);
		ConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);
		return InIndex;
	}

	template <typename OtherAllocator>
	SizeType Insert(TArray<ElementType, OtherAllocator>&& Items, const SizeType InIndex)
	{
		check((const void*)this != (const void*)&Items);
		const SizeType NumNewElements = Items.Num();
		InsertUninitialized(InIndex, NumNewElements);
		RelocateConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);
		Items.ArrayNum = 0;
		return InIndex;
	}

	SizeType Insert(const ElementType* Ptr, SizeType Count, SizeType Index)
	{
		check(Ptr != nullptr);
		InsertUninitialized(Index, Count);
		ConstructItems<ElementType>(GetData() + Index, Ptr, Count);
		return Index;
	}

	FORCEINLINE void CheckAddress(const ElementType* Addr) const
	{
		checkf(Addr < GetData() || Addr >= (GetData() + ArrayMax),
			"Attempting to use a container element (%p) which already comes from the container being modified (%p, "
			"ArrayMax: %d, ArrayNum: %d, SizeofElement: %d)!",
			(const void*)Addr, (const void*)GetData(), int(ArrayMax), int(ArrayNum), int(sizeof(ElementType)));
	}

	SizeType Insert(ElementType&& Item, SizeType Index)
	{
		CheckAddress(&Item);
		InsertUninitialized(Index, 1);
		new (GetData() + Index) ElementType(MoveTempIfPossible(Item));
		return Index;
	}

	SizeType Insert(const ElementType& Item, SizeType Index)
	{
		CheckAddress(&Item);
		InsertUninitialized(Index, 1);
		new (GetData() + Index) ElementType(Item);
		return Index;
	}

	ElementType& Insert_GetRef(ElementType&& Item, SizeType Index) // NOLINT(readability-identifier-naming): UE name
	{
		CheckAddress(&Item);
		InsertUninitialized(Index, 1);
		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(MoveTempIfPossible(Item));
		return *Ptr;
	}

	ElementType& Insert_GetRef(
		const ElementType& Item, SizeType Index) // NOLINT(readability-identifier-naming): UE name
	{
		CheckAddress(&Item);
		InsertUninitialized(Index, 1);
		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Item);
		return *Ptr;
	}

	template <typename... ArgsType>
	FORCEINLINE SizeType Emplace(ArgsType&&... Args)
	{
		const SizeType Index = AddUninitialized(1);
		new (GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
		return Index;
	}

	template <typename... ArgsType>
	FORCEINLINE ElementType& Emplace_GetRef(ArgsType&&... Args) // NOLINT(readability-identifier-naming): UE name
	{
		const SizeType Index = AddUninitialized(1);
		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Forward<ArgsType>(Args)...);
		return *Ptr;
	}

	template <typename... ArgsType>
	FORCEINLINE void EmplaceAt(SizeType Index, ArgsType&&... Args)
	{
		InsertUninitialized(Index, 1);
		new (GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
	}

	template <typename... ArgsType>
	FORCEINLINE ElementType& EmplaceAt_GetRef(
		SizeType Index, ArgsType&&... Args) // NOLINT(readability-identifier-naming): UE name
	{
		InsertUninitialized(Index, 1);
		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Forward<ArgsType>(Args)...);
		return *Ptr;
	}

	FORCEINLINE SizeType Add(ElementType&& Item)
	{
		CheckAddress(&Item);
		return Emplace(MoveTempIfPossible(Item));
	}

	FORCEINLINE SizeType Add(const ElementType& Item)
	{
		CheckAddress(&Item);
		return Emplace(Item);
	}

	FORCEINLINE ElementType& Add_GetRef(ElementType&& Item) // NOLINT(readability-identifier-naming): UE name
	{
		CheckAddress(&Item);
		return Emplace_GetRef(MoveTempIfPossible(Item));
	}

	FORCEINLINE ElementType& Add_GetRef(const ElementType& Item) // NOLINT(readability-identifier-naming): UE name
	{
		CheckAddress(&Item);
		return Emplace_GetRef(Item);
	}

	SizeType AddZeroed(SizeType Count = 1)
	{
		const SizeType Index = AddUninitialized(Count);
		FMemory::Memzero(GetData() + Index, Count * sizeof(ElementType));
		return Index;
	}

	ElementType& AddZeroed_GetRef() // NOLINT(readability-identifier-naming): UE name
	{
		const SizeType Index = AddUninitialized(1);
		ElementType* Ptr = GetData() + Index;
		FMemory::Memzero(Ptr, sizeof(ElementType));
		return *Ptr;
	}

	SizeType AddDefaulted(SizeType Count = 1)
	{
		const SizeType Index = AddUninitialized(Count);
		DefaultConstructItems<ElementType>(GetData() + Index, Count);
		return Index;
	}

	ElementType& AddDefaulted_GetRef() // NOLINT(readability-identifier-naming): UE name
	{
		const SizeType Index = AddUninitialized(1);
		ElementType* Ptr = GetData() + Index;
		DefaultConstructItems<ElementType>(Ptr, 1);
		return *Ptr;
	}

	/** Adds Item unless an equal element exists; returns the index of the element either way. */
	FORCEINLINE SizeType AddUnique(ElementType&& Item)
	{
		return AddUniqueImpl(MoveTempIfPossible(Item));
	}
	FORCEINLINE SizeType AddUnique(const ElementType& Item)
	{
		return AddUniqueImpl(Item);
	}

	template <typename OtherElementType, typename OtherAllocatorType>
	void Append(const TArray<OtherElementType, OtherAllocatorType>& Source)
	{
		check((const void*)this != (const void*)&Source);
		const SizeType SourceCount = Source.Num();
		if (!SourceCount)
		{
			return;
		}
		Reserve(ArrayNum + SourceCount);
		ConstructItems<ElementType>(GetData() + ArrayNum, Source.GetData(), SourceCount);
		ArrayNum += SourceCount;
	}

	template <typename OtherAllocatorType>
	void Append(TArray<ElementType, OtherAllocatorType>&& Source)
	{
		check((const void*)this != (const void*)&Source);
		const SizeType SourceCount = Source.Num();
		if (!SourceCount)
		{
			return;
		}
		Reserve(ArrayNum + SourceCount);
		RelocateConstructItems<ElementType>(GetData() + ArrayNum, Source.GetData(), SourceCount);
		Source.ArrayNum = 0;
		ArrayNum += SourceCount;
	}

	void Append(const ElementType* Ptr, SizeType Count)
	{
		check(Ptr != nullptr || Count == 0);
		const SizeType Pos = AddUninitialized(Count);
		ConstructItems<ElementType>(GetData() + Pos, Ptr, Count);
	}

	FORCEINLINE void Append(std::initializer_list<ElementType> InitList)
	{
		const SizeType Count = SizeType(InitList.size());
		const SizeType Pos = AddUninitialized(Count);
		ConstructItems<ElementType>(GetData() + Pos, InitList.begin(), Count);
	}

	TArray& operator+=(TArray&& Other)
	{
		Append(MoveTemp(Other));
		return *this;
	}
	TArray& operator+=(const TArray& Other)
	{
		Append(Other);
		return *this;
	}
	TArray& operator+=(std::initializer_list<ElementType> InitList)
	{
		Append(InitList);
		return *this;
	}

	/** Empties the array and fills it with Number copies of Element. */
	void Init(const ElementType& Element, SizeType Number)
	{
		Empty(Number);
		for (SizeType Index = 0; Index < Number; ++Index)
		{
			new (GetData() + Index) ElementType(Element);
		}
		ArrayNum = Number;
	}

	// Removal --------------------------------------------------------------------------------------------------------

	FORCEINLINE void RemoveAt(SizeType Index)
	{
		RemoveAtImpl(Index, 1, true);
	}

	FORCEINLINE void RemoveAt(SizeType Index, SizeType Count, bool bAllowShrinking = true)
	{
		RemoveAtImpl(Index, Count, bAllowShrinking);
	}

	/** Removes elements by moving the last ones into the hole: O(Count), does not preserve order. */
	FORCEINLINE void RemoveAtSwap(SizeType Index)
	{
		RemoveAtSwapImpl(Index, 1, true);
	}

	FORCEINLINE void RemoveAtSwap(SizeType Index, SizeType Count, bool bAllowShrinking = true)
	{
		RemoveAtSwapImpl(Index, Count, bAllowShrinking);
	}

	/** Removes every element but keeps NewSize elements' worth of memory. */
	void Reset(SizeType NewSize = 0)
	{
		if (NewSize <= ArrayMax)
		{
			DestructItems(GetData(), ArrayNum);
			ArrayNum = 0;
		}
		else
		{
			Empty(NewSize);
		}
	}

	/** Removes every element; the allocation shrinks / grows to Slack elements. */
	void Empty(SizeType Slack = 0)
	{
		DestructItems(GetData(), ArrayNum);
		checkSlow(Slack >= 0);
		ArrayNum = 0;
		if (ArrayMax != Slack)
		{
			ResizeTo(Slack);
		}
	}

	/** Resizes the array; new elements are default constructed. */
	void SetNum(SizeType NewNum, bool bAllowShrinking = true)
	{
		if (NewNum > Num())
		{
			const SizeType Diff = NewNum - ArrayNum;
			const SizeType Index = AddUninitialized(Diff);
			DefaultConstructItems<ElementType>(GetData() + Index, Diff);
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, bAllowShrinking);
		}
	}

	void SetNumZeroed(SizeType NewNum, bool bAllowShrinking = true)
	{
		if (NewNum > Num())
		{
			AddZeroed(NewNum - Num());
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, bAllowShrinking);
		}
	}

	void SetNumUninitialized(SizeType NewNum, bool bAllowShrinking = true)
	{
		if (NewNum > Num())
		{
			AddUninitialized(NewNum - Num());
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, bAllowShrinking);
		}
	}

	/** Sets the element count without constructing / destroying (NewNum <= Num). */
	void SetNumUnsafeInternal(SizeType NewNum)
	{
		checkSlow(NewNum <= Num() && NewNum >= 0);
		ArrayNum = NewNum;
	}

	/** Removes the first element equal to Item (order kept); returns 0 or 1. */
	SizeType RemoveSingle(const ElementType& Item)
	{
		const SizeType Index = Find(Item);
		if (Index == INDEX_NONE)
		{
			return 0;
		}
		ElementType* RemovePtr = GetData() + Index;
		DestructItems(RemovePtr, 1);
		RelocateConstructItems<ElementType>(RemovePtr, RemovePtr + 1, ArrayNum - (Index + 1));
		--ArrayNum;
		return 1;
	}

	/** Removes every element equal to Item (order kept); returns the number removed. */
	SizeType Remove(const ElementType& Item)
	{
		CheckAddress(&Item);
		// Element is non-const to preserve compatibility with existing code with a non-const operator==().
		return RemoveAll([&Item](ElementType& Element) { return Element == Item; });
	}

	/** Removes every element matching Predicate (order kept); returns the number removed. */
	template <class PredicateClass>
	SizeType RemoveAll(const PredicateClass& Predicate)
	{
		const SizeType OriginalNum = ArrayNum;
		SizeType WriteIndex = 0;
		ElementType* Data = GetData();
		for (SizeType ReadIndex = 0; ReadIndex < OriginalNum; ++ReadIndex)
		{
			ElementType* Read = Data + ReadIndex;
			if (::Invoke(Predicate, *Read))
			{
				DestructItem(Read);
			}
			else
			{
				if (WriteIndex != ReadIndex)
				{
					RelocateConstructItems<ElementType>(Data + WriteIndex, Read, 1);
				}
				++WriteIndex;
			}
		}
		ArrayNum = WriteIndex;
		return OriginalNum - ArrayNum;
	}

	/** Removes every element matching Predicate by swapping in the last element (order not kept). */
	template <class PredicateClass>
	SizeType RemoveAllSwap(const PredicateClass& Predicate, bool bAllowShrinking = true)
	{
		bool bRemoved = false;
		const SizeType OriginalNum = ArrayNum;
		for (SizeType ItemIndex = 0; ItemIndex < Num();)
		{
			if (::Invoke(Predicate, (*this)[ItemIndex]))
			{
				bRemoved = true;
				RemoveAtSwap(ItemIndex, 1, false);
			}
			else
			{
				++ItemIndex;
			}
		}
		if (bRemoved && bAllowShrinking)
		{
			ResizeShrink();
		}
		return OriginalNum - ArrayNum;
	}

	SizeType RemoveSingleSwap(const ElementType& Item, bool bAllowShrinking = true)
	{
		const SizeType Index = Find(Item);
		if (Index == INDEX_NONE)
		{
			return 0;
		}
		RemoveAtSwap(Index, 1, bAllowShrinking);
		return 1;
	}

	SizeType RemoveSwap(const ElementType& Item)
	{
		CheckAddress(&Item);
		const SizeType OriginalNum = ArrayNum;
		for (SizeType Index = 0; Index < ArrayNum; Index++)
		{
			if ((*this)[Index] == Item)
			{
				RemoveAtSwap(Index--);
			}
		}
		return OriginalNum - ArrayNum;
	}

	/** Swaps two elements' memory. */
	FORCEINLINE void SwapMemory(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap)
	{
		TTypeCompatibleBytes<ElementType> Temp;
		FMemory::Memcpy(&Temp, GetData() + FirstIndexToSwap, sizeof(ElementType));
		FMemory::Memcpy(GetData() + FirstIndexToSwap, GetData() + SecondIndexToSwap, sizeof(ElementType));
		FMemory::Memcpy(GetData() + SecondIndexToSwap, &Temp, sizeof(ElementType));
	}

	FORCEINLINE void Swap(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap)
	{
		check((FirstIndexToSwap >= 0) && (SecondIndexToSwap >= 0));
		check((ArrayNum > FirstIndexToSwap) && (ArrayNum > SecondIndexToSwap));
		if (FirstIndexToSwap != SecondIndexToSwap)
		{
			SwapMemory(FirstIndexToSwap, SecondIndexToSwap);
		}
	}

	/** Makes room for Number elements without changing Num. */
	FORCEINLINE void Reserve(SizeType Number)
	{
		checkSlow(Number >= 0);
		if (Number > ArrayMax)
		{
			ResizeTo(Number);
		}
	}

	// Sorting --------------------------------------------------------------------------------------------------------

	/** Unstable sort with operator<; arrays of pointers compare the pointees (UE). */
	void Sort()
	{
		::Sort(GetData(), Num());
	}

	template <class PredicateClass>
	void Sort(const PredicateClass& Predicate)
	{
		::Sort(GetData(), Num(), Predicate);
	}

	void StableSort()
	{
		::StableSort(GetData(), Num());
	}

	template <class PredicateClass>
	void StableSort(const PredicateClass& Predicate)
	{
		::StableSort(GetData(), Num(), Predicate);
	}

	// Iteration ------------------------------------------------------------------------------------------------------

	typedef TIndexedContainerIterator<TArray, ElementType, SizeType> TIterator;
	typedef TIndexedContainerIterator<const TArray, const ElementType, SizeType> TConstIterator;

	TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

#if DO_CHECK
	typedef TCheckedPointerIterator<ElementType, SizeType> RangedForIteratorType;
	typedef TCheckedPointerIterator<const ElementType, SizeType> RangedForConstIteratorType;
#else
	typedef ElementType* RangedForIteratorType;
	typedef const ElementType* RangedForConstIteratorType;
#endif

	// Ranged-for support (lower-case names required by the language).
#if DO_CHECK
	FORCEINLINE RangedForIteratorType begin()
	{
		return RangedForIteratorType(ArrayNum, GetData());
	}
	FORCEINLINE RangedForConstIteratorType begin() const
	{
		return RangedForConstIteratorType(ArrayNum, GetData());
	}
	FORCEINLINE RangedForIteratorType end()
	{
		return RangedForIteratorType(ArrayNum, GetData() + Num());
	}
	FORCEINLINE RangedForConstIteratorType end() const
	{
		return RangedForConstIteratorType(ArrayNum, GetData() + Num());
	}
#else
	FORCEINLINE RangedForIteratorType begin()
	{
		return GetData();
	}
	FORCEINLINE RangedForConstIteratorType begin() const
	{
		return GetData();
	}
	FORCEINLINE RangedForIteratorType end()
	{
		return GetData() + Num();
	}
	FORCEINLINE RangedForConstIteratorType end() const
	{
		return GetData() + Num();
	}
#endif

	// Heap -----------------------------------------------------------------------------------------------------------

	/** Builds a binary heap in place; with TLess the smallest element is at index 0 (UE: Heapify). */
	template <class PredicateClass>
	FORCEINLINE void Heapify(const PredicateClass& Predicate)
	{
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		AlgoImpl::HeapifyInternal(GetData(), Num(), FIdentityFunctor(), PredicateWrapper);
	}
	void Heapify()
	{
		Heapify(TLess<ElementType>());
	}

	template <class PredicateClass>
	SizeType HeapPush(ElementType&& InItem, const PredicateClass& Predicate)
	{
		Add(MoveTempIfPossible(InItem));
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		return AlgoImpl::HeapSiftUp(GetData(), 0, Num() - 1, FIdentityFunctor(), PredicateWrapper);
	}
	template <class PredicateClass>
	SizeType HeapPush(const ElementType& InItem, const PredicateClass& Predicate)
	{
		Add(InItem);
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		return AlgoImpl::HeapSiftUp(GetData(), 0, Num() - 1, FIdentityFunctor(), PredicateWrapper);
	}
	SizeType HeapPush(ElementType&& InItem)
	{
		return HeapPush(MoveTempIfPossible(InItem), TLess<ElementType>());
	}
	SizeType HeapPush(const ElementType& InItem)
	{
		return HeapPush(InItem, TLess<ElementType>());
	}

	template <class PredicateClass>
	void HeapPop(ElementType& OutItem, const PredicateClass& Predicate, bool bAllowShrinking = true)
	{
		OutItem = MoveTemp((*this)[0]);
		RemoveAtSwap(0, 1, bAllowShrinking);
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), 0, Num(), FIdentityFunctor(), PredicateWrapper);
	}
	void HeapPop(ElementType& OutItem, bool bAllowShrinking = true)
	{
		HeapPop(OutItem, TLess<ElementType>(), bAllowShrinking);
	}

	template <class PredicateClass>
	void HeapPopDiscard(const PredicateClass& Predicate, bool bAllowShrinking = true)
	{
		RemoveAtSwap(0, 1, bAllowShrinking);
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), 0, Num(), FIdentityFunctor(), PredicateWrapper);
	}
	void HeapPopDiscard(bool bAllowShrinking = true)
	{
		HeapPopDiscard(TLess<ElementType>(), bAllowShrinking);
	}

	const ElementType& HeapTop() const
	{
		return (*this)[0];
	}
	ElementType& HeapTop()
	{
		return (*this)[0];
	}

	template <class PredicateClass>
	void HeapRemoveAt(SizeType Index, const PredicateClass& Predicate, bool bAllowShrinking = true)
	{
		RemoveAtSwap(Index, 1, bAllowShrinking);
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), Index, Num(), FIdentityFunctor(), PredicateWrapper);
		AlgoImpl::HeapSiftUp(GetData(), 0, FMath::Min(Index, Num() - 1), FIdentityFunctor(), PredicateWrapper);
	}
	void HeapRemoveAt(SizeType Index, bool bAllowShrinking = true)
	{
		HeapRemoveAt(Index, TLess<ElementType>(), bAllowShrinking);
	}

	template <class PredicateClass>
	void HeapSort(const PredicateClass& Predicate)
	{
		TDereferenceWrapper<ElementType, PredicateClass> PredicateWrapper(Predicate);
		AlgoImpl::HeapSortInternal(GetData(), Num(), FIdentityFunctor(), PredicateWrapper);
	}
	void HeapSort()
	{
		HeapSort(TLess<ElementType>());
	}

	const ElementAllocatorType& GetAllocatorInstance() const
	{
		return AllocatorInstance;
	}
	ElementAllocatorType& GetAllocatorInstance()
	{
		return AllocatorInstance;
	}

private:
	template <typename ArgsType>
	SizeType AddUniqueImpl(ArgsType&& Args)
	{
		SizeType Index;
		if (Find(Args, Index))
		{
			return Index;
		}
		return Add(Forward<ArgsType>(Args));
	}

	/** Moves Other into To when the allocators allow it, else copies (the UE MoveOrCopy). */
	template <typename FromArrayType, typename ToArrayType>
	static FORCEINLINE void MoveOrCopy(ToArrayType& ToArray, FromArrayType& FromArray, SizeType PrevMax)
	{
		using FromAllocatorType = typename FromArrayType::AllocatorType;
		using ToAllocatorType = typename ToArrayType::AllocatorType;
		using FromElementType = typename FromArrayType::ElementType;
		using ToElementType = typename ToArrayType::ElementType;

		if constexpr (std::is_same_v<FromAllocatorType, ToAllocatorType> &&
			TAllocatorTraits<FromAllocatorType>::SupportsMove &&
			std::is_same_v<std::remove_cv_t<FromElementType>, std::remove_cv_t<ToElementType>>)
		{
			ToArray.AllocatorInstance.MoveToEmpty(FromArray.AllocatorInstance);

			ToArray.ArrayNum = FromArray.ArrayNum;
			ToArray.ArrayMax = FromArray.ArrayMax;
			FromArray.ArrayNum = 0;
			FromArray.ArrayMax = FromArray.AllocatorInstance.GetInitialCapacity();
		}
		else
		{
			ToArray.CopyToEmpty(FromArray.GetData(), FromArray.Num(), PrevMax, 0);
		}
	}

	FORCENOINLINE void ResizeGrow(SizeType OldNum)
	{
		ArrayMax = AllocatorInstance.CalculateSlackGrow(ArrayNum, ArrayMax, sizeof(ElementType));
		AllocatorInstance.ResizeAllocation(OldNum, ArrayMax, sizeof(ElementType), GetContainerAlignment<ElementType>());
	}

	FORCENOINLINE void ResizeShrink()
	{
		const SizeType NewArrayMax = AllocatorInstance.CalculateSlackShrink(ArrayNum, ArrayMax, sizeof(ElementType));
		if (NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			check(ArrayMax >= ArrayNum);
			AllocatorInstance.ResizeAllocation(
				ArrayNum, ArrayMax, sizeof(ElementType), GetContainerAlignment<ElementType>());
		}
	}

	FORCENOINLINE void ResizeTo(SizeType NewMax)
	{
		if (NewMax)
		{
			NewMax = AllocatorInstance.CalculateSlackReserve(NewMax, sizeof(ElementType));
		}
		if (NewMax != ArrayMax)
		{
			ArrayMax = NewMax;
			AllocatorInstance.ResizeAllocation(
				ArrayNum, ArrayMax, sizeof(ElementType), GetContainerAlignment<ElementType>());
		}
	}

	FORCENOINLINE void ResizeForCopy(SizeType NewMax, SizeType PrevMax)
	{
		if (NewMax)
		{
			NewMax = AllocatorInstance.CalculateSlackReserve(NewMax, sizeof(ElementType));
		}
		if (NewMax != PrevMax)
		{
			AllocatorInstance.ResizeAllocation(0, NewMax, sizeof(ElementType), GetContainerAlignment<ElementType>());
		}
		ArrayMax = NewMax;
	}

	/** Copies OtherNum elements into this array, which holds no constructed elements. */
	template <typename OtherElementType, typename OtherSizeType>
	void CopyToEmpty(const OtherElementType* OtherData, OtherSizeType OtherNum, SizeType PrevMax, SizeType ExtraSlack)
	{
		const SizeType NewNum = static_cast<SizeType>(OtherNum);
		checkf(static_cast<OtherSizeType>(NewNum) == OtherNum,
			"Invalid number of elements to add to this array type: %d", int(NewNum));

		checkSlow(ExtraSlack >= 0);
		ArrayNum = NewNum;
		if (OtherNum || ExtraSlack || PrevMax)
		{
			ResizeForCopy(NewNum + ExtraSlack, PrevMax);
			ConstructItems<ElementType>(GetData(), OtherData, OtherNum);
		}
		else
		{
			ArrayMax = AllocatorInstance.GetInitialCapacity();
		}
	}

	void RemoveAtImpl(SizeType Index, SizeType Count, bool bAllowShrinking)
	{
		if (Count)
		{
			CheckInvariants();
			checkSlow((Count >= 0) & (Index >= 0) & (Index + Count <= ArrayNum));

			DestructItems(GetData() + Index, Count);

			// Skip memmove in the common case that there is nothing to move.
			const SizeType NumToMove = ArrayNum - Index - Count;
			if (NumToMove)
			{
				RelocateConstructItems<ElementType>(GetData() + Index, GetData() + Index + Count, NumToMove);
			}
			ArrayNum -= Count;

			if (bAllowShrinking)
			{
				ResizeShrink();
			}
		}
	}

	void RemoveAtSwapImpl(SizeType Index, SizeType Count, bool bAllowShrinking)
	{
		if (Count)
		{
			CheckInvariants();
			checkSlow((Count >= 0) & (Index >= 0) & (Index + Count <= ArrayNum));

			DestructItems(GetData() + Index, Count);

			// Replace the elements in the hole created by the removal with elements from the end of the array.
			const SizeType NumElementsInHole = Count;
			const SizeType NumElementsAfterHole = ArrayNum - (Index + Count);
			const SizeType NumElementsToMoveIntoHole = FMath::Min(NumElementsInHole, NumElementsAfterHole);
			if (NumElementsToMoveIntoHole)
			{
				RelocateConstructItems<ElementType>(
					GetData() + Index, GetData() + (ArrayNum - NumElementsToMoveIntoHole), NumElementsToMoveIntoHole);
			}
			ArrayNum -= Count;

			if (bAllowShrinking)
			{
				ResizeShrink();
			}
		}
	}

	ElementAllocatorType AllocatorInstance;
	SizeType ArrayNum;
	SizeType ArrayMax;
};

template <typename InElementType, typename Allocator>
struct TIsZeroConstructType<TArray<InElementType, Allocator>>
{
	enum
	{
		Value = TAllocatorTraits<Allocator>::IsZeroConstruct
	};
};

template <typename InElementType, typename Allocator>
struct TIsContiguousContainer<TArray<InElementType, Allocator>>
{
	enum
	{
		Value = true
	};
};
