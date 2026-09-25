#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/SparseArray.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Sorting.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <cstddef>
#include <initializer_list>
#include <new>
#include <type_traits>

/** Key access / comparison / hashing of a set element (UE: BaseKeyFuncs). */
template <typename ElementType, typename InKeyType, bool bInAllowDuplicateKeys = false>
struct BaseKeyFuncs
{
	typedef InKeyType KeyType;
	typedef typename TCallTraits<InKeyType>::ParamType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	enum
	{
		bAllowDuplicateKeys = bInAllowDuplicateKeys
	};
};

/** Elements are their own keys (UE: DefaultKeyFuncs). */
template <typename ElementType, bool bInAllowDuplicateKeys /* = false */>
struct DefaultKeyFuncs : BaseKeyFuncs<ElementType, ElementType, bInAllowDuplicateKeys>
{
	typedef typename TCallTraits<ElementType>::ParamType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element;
	}

	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A == B;
	}

	template <typename ComparableKey>
	static FORCEINLINE bool Matches(KeyInitType A, ComparableKey B)
	{
		return A == B;
	}

	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}

	template <typename ComparableKey>
	static FORCEINLINE uint32 GetKeyHash(ComparableKey Key)
	{
		return GetTypeHash(Key);
	}
};

/** Moves Source into Dest by relocation: Dest is destroyed, Source's bytes move and Source is left unconstructed. */
template <typename T>
FORCEINLINE void MoveByRelocate(T& A, T& B)
{
	// Destruct the previous value of A.
	A.~T();
	// Relocate B into the 'hole' left by the destruction of A, leaving a hole in B instead.
	RelocateConstructItems<T>(&A, &B, 1);
}

/** Id of an element in a TSet (UE: FSetElementId). */
class FSetElementId
{
public:
	template <typename, typename, typename>
	friend class TSet;

	FORCEINLINE FSetElementId()
		: Index(INDEX_NONE)
	{
	}

	FORCEINLINE bool IsValidId() const
	{
		return Index != INDEX_NONE;
	}

	FORCEINLINE friend bool operator==(const FSetElementId& A, const FSetElementId& B)
	{
		return A.Index == B.Index;
	}
	FORCEINLINE friend bool operator!=(const FSetElementId& A, const FSetElementId& B)
	{
		return A.Index != B.Index;
	}

	FORCEINLINE int32 AsInteger() const
	{
		return Index;
	}

	FORCEINLINE static FSetElementId FromInteger(int32 Integer)
	{
		return FSetElementId(Integer);
	}

private:
	int32 Index;

	FORCEINLINE explicit FSetElementId(int32 InIndex)
		: Index(InIndex)
	{
	}

	FORCEINLINE operator int32() const
	{
		return Index;
	}
};

/** Element storage of a TSet: the value plus its hash chain link (UE: TSetElement). */
template <typename InElementType>
class TSetElement
{
public:
	typedef InElementType ElementType;

	/** Constructs the value from InValue (in place). */
	template <typename InitType>
	explicit FORCEINLINE TSetElement(InitType&& InValue)
		: Value(Forward<InitType>(InValue))
	{
	}

	TSetElement(TSetElement&&) = default;
	TSetElement(const TSetElement&) = default;
	TSetElement& operator=(TSetElement&&) = default;
	TSetElement& operator=(const TSetElement&) = default;

	FORCEINLINE bool operator==(const TSetElement& Other) const
	{
		return Value == Other.Value;
	}
	FORCEINLINE bool operator!=(const TSetElement& Other) const
	{
		return Value != Other.Value;
	}

	ElementType Value;
	mutable FSetElementId HashNextId;
	mutable int32 HashIndex = 0;
};

/**
 * Hash set (UE: TSet). Elements live in a TSparseArray (stable ids, iteration in index order); the hash buckets hold
 * element ids chained through TSetElement::HashNextId. KeyFuncs selects the key, its hash and equality.
 */
template <typename InElementType, typename KeyFuncs /* = DefaultKeyFuncs<ElementType> */,
	typename Allocator /* = FDefaultSetAllocator */>
class TSet
{
public:
	typedef InElementType ElementType;
	typedef typename KeyFuncs::KeyInitType KeyInitType;
	typedef typename KeyFuncs::ElementInitType ElementInitType;
	typedef TSetElement<InElementType> SetElementType;

private:
	template <typename, typename, typename>
	friend class TSet;
	// Checks that its layout matches (TScriptSet::CheckConstraints).
	template <typename>
	friend class TScriptSet;

	typedef TSparseArray<SetElementType, typename Allocator::SparseArrayAllocator> ElementArrayType;
	typedef typename Allocator::HashAllocator::template ForElementType<FSetElementId> HashType;

public:
	FORCEINLINE TSet()
		: HashSize(0)
	{
	}

	FORCEINLINE TSet(const TSet& Copy)
		: HashSize(0)
	{
		*this = Copy;
	}

	FORCEINLINE explicit TSet(TArrayView<const ElementType> InArrayView)
		: HashSize(0)
	{
		Append(InArrayView);
	}

	FORCEINLINE explicit TSet(TArray<ElementType>&& InArray)
		: HashSize(0)
	{
		Append(MoveTemp(InArray));
	}

	TSet(std::initializer_list<ElementType> InitList)
		: HashSize(0)
	{
		Append(InitList);
	}

	TSet(TSet&& Other)
		: HashSize(0)
	{
		MoveOrCopy(*this, Other);
	}

	FORCEINLINE ~TSet()
	{
		HashSize = 0;
	}

	TSet& operator=(const TSet& Copy)
	{
		if (this != &Copy)
		{
			const int32 CopyHashSize = Copy.HashSize;

			DestructItems((FSetElementId*)Hash.GetAllocation(), HashSize);
			Hash.ResizeAllocation(0, CopyHashSize, sizeof(FSetElementId));
			ConstructItems<FSetElementId>(
				Hash.GetAllocation(), (FSetElementId*)Copy.Hash.GetAllocation(), CopyHashSize);
			HashSize = CopyHashSize;

			Elements = Copy.Elements;
		}
		return *this;
	}

	TSet& operator=(TSet&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}
		return *this;
	}

	TSet& operator=(std::initializer_list<ElementType> InitList)
	{
		Reset();
		Append(InitList);
		return *this;
	}

	/** Removes every element; storage and hash shrink / grow to ExpectedNumElements. */
	void Empty(int32 ExpectedNumElements = 0)
	{
		// Calculate the desired hash size for the specified number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(ExpectedNumElements);
		const bool bShouldDoRehash = ShouldRehash(ExpectedNumElements, DesiredHashSize, true);

		if (!bShouldDoRehash)
		{
			// If the hash was already the desired size, clear the references to the elements that are now empty.
			UnhashElements();
		}

		// Empty the elements array, and reallocate it for the expected number of elements.
		Elements.Empty(ExpectedNumElements);

		// Resize the hash to the desired size for the expected number of elements.
		if (bShouldDoRehash)
		{
			HashSize = DesiredHashSize;
			Rehash();
		}
	}

	/** Removes every element, keeping the storage. */
	void Reset()
	{
		if (Num() == 0)
		{
			return;
		}

		// Reset the elements array.
		UnhashElements();
		Elements.Reset();
	}

	/** Releases the slack. */
	FORCEINLINE void Shrink()
	{
		Elements.Shrink();
		Relax();
	}

	/** Compacts the elements (order not kept) and rehashes. */
	FORCEINLINE void Compact()
	{
		if (Elements.Compact())
		{
			HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
			Rehash();
		}
	}

	/** Compacts the elements keeping their order and rehashes. */
	FORCEINLINE void CompactStable()
	{
		if (Elements.CompactStable())
		{
			HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
			Rehash();
		}
	}

	FORCEINLINE void Reserve(int32 Number)
	{
		// Makes sense only when Number > Elements.Num() since TSparseArray::Reserve does any work only if that's the
		// case.
		if (Number > Elements.Num())
		{
			// Preallocates memory for array of elements.
			Elements.Reserve(Number);

			// Calculate the corresponding hash size for the specified number of elements.
			const int32 NewHashSize = Allocator::GetNumberOfHashBuckets(Number);

			// If the hash hasn't been created yet, or is smaller than the corresponding hash size, rehash to force a
			// preallocation.
			if (!HashSize || HashSize < NewHashSize)
			{
				HashSize = NewHashSize;
				Rehash();
			}
		}
	}

	/** Shrinks the hash to the size the element count needs. */
	FORCEINLINE void Relax()
	{
		ConditionalRehash(Elements.Num(), true);
	}

	SIZE_T GetAllocatedSize() const
	{
		return Elements.GetAllocatedSize() + (HashSize * sizeof(FSetElementId));
	}

	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Elements.Num() == 0;
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Elements.GetMaxIndex();
	}

	FORCEINLINE bool IsValidId(FSetElementId Id) const
	{
		return Id.IsValidId() && Id >= 0 && Id < Elements.GetMaxIndex() && Elements.IsAllocated(Id);
	}

	FORCEINLINE ElementType& operator[](FSetElementId Id)
	{
		return Elements[Id].Value;
	}
	FORCEINLINE const ElementType& operator[](FSetElementId Id) const
	{
		return Elements[Id].Value;
	}

	/** Adds an element, replacing an equal key unless duplicates are allowed; returns its id. */
	FORCEINLINE FSetElementId Add(const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return Emplace(InElement, bIsAlreadyInSetPtr);
	}
	FORCEINLINE FSetElementId Add(InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return Emplace(MoveTempIfPossible(InElement), bIsAlreadyInSetPtr);
	}

	FORCEINLINE FSetElementId AddByHash(
		uint32 KeyHash, const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return EmplaceByHash(KeyHash, InElement, bIsAlreadyInSetPtr);
	}
	FORCEINLINE FSetElementId AddByHash(uint32 KeyHash, InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return EmplaceByHash(KeyHash, MoveTempIfPossible(InElement), bIsAlreadyInSetPtr);
	}

	/** Constructs an element from Args (UE: Emplace). */
	template <typename ArgsType>
	FSetElementId Emplace(ArgsType&& Args, bool* bIsAlreadyInSetPtr = nullptr)
	{
		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		SetElementType& Element = *new (ElementAllocation) SetElementType(Forward<ArgsType>(Args));

		const uint32 KeyHash = KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(Element.Value));
		return EmplaceImpl(KeyHash, Element, FSetElementId(ElementAllocation.Index), bIsAlreadyInSetPtr);
	}

	template <typename ArgsType>
	FSetElementId EmplaceByHash(uint32 KeyHash, ArgsType&& Args, bool* bIsAlreadyInSetPtr = nullptr)
	{
		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		SetElementType& Element = *new (ElementAllocation) SetElementType(Forward<ArgsType>(Args));

		return EmplaceImpl(KeyHash, Element, FSetElementId(ElementAllocation.Index), bIsAlreadyInSetPtr);
	}

	template <typename ArrayAllocator>
	void Append(const TArray<ElementType, ArrayAllocator>& InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (const ElementType& Element : InElements)
		{
			Add(Element);
		}
	}

	template <typename ArrayAllocator>
	void Append(TArray<ElementType, ArrayAllocator>&& InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (ElementType& Element : InElements)
		{
			Add(MoveTempIfPossible(Element));
		}
		InElements.Reset();
	}

	void Append(TArrayView<const ElementType> InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (const ElementType& Element : InElements)
		{
			Add(Element);
		}
	}

	template <typename OtherAllocator>
	void Append(const TSet<ElementType, KeyFuncs, OtherAllocator>& OtherSet)
	{
		Reserve(Elements.Num() + OtherSet.Num());
		for (const ElementType& Element : OtherSet)
		{
			Add(Element);
		}
	}

	template <typename OtherAllocator>
	void Append(TSet<ElementType, KeyFuncs, OtherAllocator>&& OtherSet)
	{
		Reserve(Elements.Num() + OtherSet.Num());
		for (ElementType& Element : OtherSet)
		{
			Add(MoveTempIfPossible(Element));
		}
		OtherSet.Reset();
	}

	void Append(std::initializer_list<ElementType> InitList)
	{
		Reserve(Elements.Num() + int32(InitList.size()));
		for (const ElementType& Element : InitList)
		{
			Add(Element);
		}
	}

	/** Removes the element with the given id. */
	void Remove(FSetElementId ElementId)
	{
		if (Elements.Num())
		{
			const SetElementType& ElementBeingRemoved = Elements[ElementId];

			// Remove the element from the hash.
			for (FSetElementId* NextElementId = &GetTypedHash(ElementBeingRemoved.HashIndex);
				NextElementId->IsValidId(); NextElementId = &Elements[*NextElementId].HashNextId)
			{
				if (*NextElementId == ElementId)
				{
					*NextElementId = ElementBeingRemoved.HashNextId;
					break;
				}
			}
		}

		// Remove the element from the elements array.
		Elements.RemoveAt(ElementId);
	}

	/** Removes the elements matching Key; returns how many were removed. */
	int32 Remove(KeyInitType Key)
	{
		if (Elements.Num())
		{
			return RemoveImpl(KeyFuncs::GetKeyHash(Key), Key);
		}
		return 0;
	}

	template <typename ComparableKey>
	int32 RemoveByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		checkSlow(KeyHash == KeyFuncs::GetKeyHash(Key));
		if (Elements.Num())
		{
			return RemoveImpl(KeyHash, Key);
		}
		return 0;
	}

	/** Id of the element matching Key, or an invalid id. */
	FSetElementId FindId(KeyInitType Key) const
	{
		if (Elements.Num())
		{
			for (FSetElementId ElementId = GetTypedHash(KeyFuncs::GetKeyHash(Key)); ElementId.IsValidId();
				ElementId = Elements[ElementId].HashNextId)
			{
				if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Elements[ElementId].Value), Key))
				{
					return ElementId;
				}
			}
		}
		return FSetElementId();
	}

	template <typename ComparableKey>
	FSetElementId FindIdByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		if (Elements.Num())
		{
			checkSlow(KeyHash == KeyFuncs::GetKeyHash(Key));
			for (FSetElementId ElementId = GetTypedHash(KeyHash); ElementId.IsValidId();
				ElementId = Elements[ElementId].HashNextId)
			{
				if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Elements[ElementId].Value), Key))
				{
					return ElementId;
				}
			}
		}
		return FSetElementId();
	}

	FORCEINLINE ElementType* Find(KeyInitType Key)
	{
		const FSetElementId ElementId = FindId(Key);
		return ElementId.IsValidId() ? &Elements[ElementId].Value : nullptr;
	}
	FORCEINLINE const ElementType* Find(KeyInitType Key) const
	{
		return const_cast<TSet*>(this)->Find(Key);
	}

	template <typename ComparableKey>
	ElementType* FindByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		const FSetElementId ElementId = FindIdByHash(KeyHash, Key);
		return ElementId.IsValidId() ? &Elements[ElementId].Value : nullptr;
	}
	template <typename ComparableKey>
	const ElementType* FindByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		return const_cast<TSet*>(this)->FindByHash(KeyHash, Key);
	}

	/** Any element, or nullptr when empty. */
	FORCEINLINE ElementType* FindArbitraryElement()
	{
		const int32 Result = Elements.GetMaxIndex() > 0 ? TConstSetBitIteratorFirst() : INDEX_NONE;
		return Result != INDEX_NONE ? &Elements[Result].Value : nullptr;
	}

	FORCEINLINE bool Contains(KeyInitType Key) const
	{
		return FindId(Key).IsValidId();
	}

	template <typename ComparableKey>
	FORCEINLINE bool ContainsByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		return FindIdByHash(KeyHash, Key).IsValidId();
	}

	/** Sorts the elements (unstable) and rehashes. */
	template <typename PredicateType>
	void Sort(const PredicateType& Predicate)
	{
		// Sort the elements according to the provided comparison class.
		Elements.Sort(FElementCompareClass<PredicateType>(Predicate));

		// Rehash.
		Rehash();
	}

	template <typename PredicateType>
	void StableSort(const PredicateType& Predicate)
	{
		Elements.StableSort(FElementCompareClass<PredicateType>(Predicate));
		Rehash();
	}

	/** Elements in both sets. */
	TSet Intersect(const TSet& OtherSet) const
	{
		const bool bOtherSmaller = (Num() > OtherSet.Num());
		const TSet& A = (bOtherSmaller ? OtherSet : *this);
		const TSet& B = (bOtherSmaller ? *this : OtherSet);

		TSet Result;
		Result.Reserve(A.Num()); // Worst case is everything in smaller is in larger

		for (TConstIterator SetIt(A); SetIt; ++SetIt)
		{
			if (B.Contains(KeyFuncs::GetSetKey(*SetIt)))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/** Elements in either set. */
	TSet Union(const TSet& OtherSet) const
	{
		TSet Result;
		Result.Reserve(Num() + OtherSet.Num()); // Worst case is 2 totally unique Sets

		for (TConstIterator SetIt(*this); SetIt; ++SetIt)
		{
			Result.Add(*SetIt);
		}
		for (TConstIterator SetIt(OtherSet); SetIt; ++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

	/** Elements of this set that are not in OtherSet. */
	TSet Difference(const TSet& OtherSet) const
	{
		TSet Result;
		Result.Reserve(Num()); // Worst case is no elements of this are in Other

		for (TConstIterator SetIt(*this); SetIt; ++SetIt)
		{
			if (!OtherSet.Contains(KeyFuncs::GetSetKey(*SetIt)))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/** True when every element of OtherSet is in this set. */
	bool Includes(const TSet& OtherSet) const
	{
		bool bIncludesSet = true;
		if (OtherSet.Num() <= Num())
		{
			for (TConstIterator OtherSetIt(OtherSet); OtherSetIt; ++OtherSetIt)
			{
				if (!Contains(KeyFuncs::GetSetKey(*OtherSetIt)))
				{
					bIncludesSet = false;
					break;
				}
			}
		}
		else
		{
			// Not possible to include if it is bigger than us.
			bIncludesSet = false;
		}
		return bIncludesSet;
	}

	/** The elements as an array. */
	TArray<ElementType> Array() const
	{
		TArray<ElementType> Result;
		Result.Reserve(Num());
		for (TConstIterator SetIt(*this); SetIt; ++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

private:
	/** Iterator over the elements; RemoveCurrent is safe (UE: TSet::TBaseIterator). */
	template <bool bConst>
	class TBaseIterator
	{
	private:
		friend class TSet;

		typedef std::conditional_t<bConst, const ElementType, ElementType> ItElementType;
		typedef std::conditional_t<bConst, typename ElementArrayType::TConstIterator,
			typename ElementArrayType::TIterator>
			ElementItType;

	public:
		FORCEINLINE TBaseIterator(const ElementItType& InElementIt)
			: ElementIt(InElementIt)
		{
		}

		FORCEINLINE TBaseIterator& operator++()
		{
			++ElementIt;
			return *this;
		}

		FORCEINLINE explicit operator bool() const
		{
			return !!ElementIt;
		}

		FORCEINLINE bool operator==(const TBaseIterator& Rhs) const
		{
			return ElementIt == Rhs.ElementIt;
		}
		FORCEINLINE bool operator!=(const TBaseIterator& Rhs) const
		{
			return ElementIt != Rhs.ElementIt;
		}

		FORCEINLINE FSetElementId GetId() const
		{
			return FSetElementId::FromInteger(ElementIt.GetIndex());
		}

		FORCEINLINE ItElementType* operator->() const
		{
			return &ElementIt->Value;
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return ElementIt->Value;
		}

	protected:
		ElementItType ElementIt;
	};

	/** Iterates the elements matching a key (UE: TSet::TBaseKeyIterator). */
	template <bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef std::conditional_t<bConst, const TSet, TSet> SetType;
		typedef std::conditional_t<bConst, const ElementType, ElementType> ItElementType;

	public:
		FORCEINLINE TBaseKeyIterator(SetType& InSet, KeyInitType InKey)
			: Set(InSet)
			, Key(InKey)
			, Id()
		{
			// The set's hash needs to be initialized to find the elements with the specified key.
			Set.ConditionalRehash(Set.Elements.Num());
			if (Set.HashSize)
			{
				NextId = Set.GetTypedHash(KeyFuncs::GetKeyHash(Key));
				++(*this);
			}
		}

		FORCEINLINE TBaseKeyIterator& operator++()
		{
			Id = NextId;

			while (Id.IsValidId())
			{
				NextId = Set.GetInternalElement(Id).HashNextId;
				checkSlow(Id != NextId);

				if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Set[Id]), Key))
				{
					break;
				}

				Id = NextId;
			}
			return *this;
		}

		FORCEINLINE explicit operator bool() const
		{
			return Id.IsValidId();
		}

		FORCEINLINE ItElementType* operator->() const
		{
			return &Set[Id];
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return Set[Id];
		}

	protected:
		SetType& Set;
		typename TRemoveReference<KeyInitType>::Type Key;
		FSetElementId Id;
		FSetElementId NextId;
	};

public:
	class TConstIterator : public TBaseIterator<true>
	{
		friend class TSet;

	public:
		FORCEINLINE TConstIterator(const TSet& InSet)
			: TBaseIterator<true>(InSet.Elements.begin())
		{
		}

	private:
		FORCEINLINE TConstIterator(const typename ElementArrayType::TConstIterator& InIt)
			: TBaseIterator<true>(InIt)
		{
		}
	};

	class TIterator : public TBaseIterator<false>
	{
		friend class TSet;

	public:
		FORCEINLINE TIterator(TSet& InSet)
			: TBaseIterator<false>(InSet.Elements.begin())
			, Set(InSet)
		{
		}

		/** Removes the current element; iteration stays valid. */
		FORCEINLINE void RemoveCurrent()
		{
			Set.Remove(TBaseIterator<false>::GetId());
		}

	private:
		FORCEINLINE TIterator(TSet& InSet, const typename ElementArrayType::TIterator& InIt)
			: TBaseIterator<false>(InIt)
			, Set(InSet)
		{
		}

		TSet& Set;
	};

	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	public:
		FORCEINLINE TConstKeyIterator(const TSet& InSet, KeyInitType InKey)
			: TBaseKeyIterator<true>(InSet, InKey)
		{
		}
	};

	class TKeyIterator : public TBaseKeyIterator<false>
	{
	public:
		FORCEINLINE TKeyIterator(TSet& InSet, KeyInitType InKey)
			: TBaseKeyIterator<false>(InSet, InKey)
		{
		}

		/** Removes the current element; iteration stays valid. */
		FORCEINLINE void RemoveCurrent()
		{
			this->Set.Remove(TBaseKeyIterator<false>::Id);
			TBaseKeyIterator<false>::Id = FSetElementId();
		}
	};

	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	// Ranged-for support (lower-case names required by the language).
	FORCEINLINE TIterator begin()
	{
		return TIterator(*this, Elements.begin());
	}
	FORCEINLINE TConstIterator begin() const
	{
		return TConstIterator(Elements.begin());
	}
	FORCEINLINE TIterator end()
	{
		return TIterator(*this, Elements.end());
	}
	FORCEINLINE TConstIterator end() const
	{
		return TConstIterator(Elements.end());
	}

private:
	template <typename PredicateType>
	class FElementCompareClass
	{
		const PredicateType& Predicate;

	public:
		FORCEINLINE FElementCompareClass(const PredicateType& InPredicate)
			: Predicate(InPredicate)
		{
		}

		FORCEINLINE bool operator()(const SetElementType& A, const SetElementType& B) const
		{
			return Predicate(A.Value, B.Value);
		}
	};

	FORCEINLINE int32 TConstSetBitIteratorFirst() const
	{
		typename ElementArrayType::TConstIterator It = Elements.begin();
		return It ? It.GetIndex() : INDEX_NONE;
	}

	template <typename ComparableKey>
	int32 RemoveImpl(uint32 KeyHash, const ComparableKey& Key)
	{
		int32 NumRemovedElements = 0;

		FSetElementId* NextElementId = &GetTypedHash(KeyHash);
		while (NextElementId->IsValidId())
		{
			const int32 NextIndex = NextElementId->AsInteger();
			SetElementType& Element = Elements[NextIndex];

			if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Element.Value), Key))
			{
				// This element matches the key, remove it from the set. Note that Remove sets *NextElementId to point
				// to the next element after the removed element in the hash bucket.
				*NextElementId = Element.HashNextId;
				Elements.RemoveAt(NextIndex);
				NumRemovedElements++;

				if (!KeyFuncs::bAllowDuplicateKeys)
				{
					// If the hash disallows duplicate keys, we're done removing after the first matched key.
					break;
				}
			}
			else
			{
				NextElementId = &Element.HashNextId;
			}
		}

		return NumRemovedElements;
	}

	FSetElementId EmplaceImpl(
		uint32 KeyHash, SetElementType& Element, FSetElementId ElementId, bool* bIsAlreadyInSetPtr)
	{
		bool bIsAlreadyInSet = false;
		if constexpr (!KeyFuncs::bAllowDuplicateKeys)
		{
			// If the set doesn't allow duplicate keys, check for an existing element with the same key as the element
			// being added. Don't bother searching for a duplicate if this is the first element we're adding.
			if (Elements.Num() != 1)
			{
				const FSetElementId ExistingId = FindIdByHash(KeyHash, KeyFuncs::GetSetKey(Element.Value));
				bIsAlreadyInSet = ExistingId.IsValidId();
				if (bIsAlreadyInSet)
				{
					// If there's an existing element with the same key as the new element, replace the existing element
					// with the new element.
					MoveByRelocate(Elements[ExistingId].Value, Element.Value);

					// Then remove the new element.
					Elements.RemoveAtUninitialized(ElementId);

					// Then point the return value at the replaced element.
					ElementId = ExistingId;
				}
			}
		}

		if (!bIsAlreadyInSet)
		{
			// Check if the hash needs to be resized.
			if (!ConditionalRehash(Elements.Num()))
			{
				// If the rehash didn't add the new element to the hash, add it.
				LinkElement(ElementId, Element, KeyHash);
			}
		}

		if (bIsAlreadyInSetPtr)
		{
			*bIsAlreadyInSetPtr = bIsAlreadyInSet;
		}

		return ElementId;
	}

	/** Moves when the allocators allow it, else copies. */
	static FORCEINLINE void MoveOrCopy(TSet& ToSet, TSet& FromSet)
	{
		ToSet.Elements = MoveTemp(FromSet.Elements);

		ToSet.Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));
		if constexpr (TAllocatorTraits<typename Allocator::HashAllocator>::SupportsMove)
		{
			ToSet.Hash.MoveToEmpty(FromSet.Hash);
			ToSet.HashSize = FromSet.HashSize;
			FromSet.HashSize = 0;
		}
		else
		{
			ToSet.HashSize = 0;
			ToSet.Rehash();
			FromSet.Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));
			FromSet.HashSize = 0;
		}
	}

	FORCEINLINE const SetElementType& GetInternalElement(FSetElementId Id) const
	{
		return Elements[Id];
	}
	FORCEINLINE SetElementType& GetInternalElement(FSetElementId Id)
	{
		return Elements[Id];
	}

	/** Hash bucket head for a hash value. */
	FORCEINLINE FSetElementId& GetTypedHash(int32 HashIndex) const
	{
		return ((FSetElementId*)Hash.GetAllocation())[HashIndex & (HashSize - 1)];
	}

	/** Adds an element to its hash bucket. */
	FORCEINLINE void LinkElement(FSetElementId ElementId, const SetElementType& Element, uint32 KeyHash) const
	{
		// Compute the hash bucket the element goes in.
		Element.HashIndex = KeyHash & (HashSize - 1);

		// Link the element into the hash bucket.
		FSetElementId& TypedHash = GetTypedHash(Element.HashIndex);
		Element.HashNextId = TypedHash;
		TypedHash = ElementId;
	}

	FORCEINLINE void HashElement(FSetElementId ElementId, const SetElementType& Element) const
	{
		LinkElement(ElementId, Element, KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(Element.Value)));
	}

	/** Clears the hash buckets without freeing them. */
	void UnhashElements()
	{
		if (HashSize)
		{
			FSetElementId* HashPtr = (FSetElementId*)Hash.GetAllocation();
			for (int32 Index = 0; Index < HashSize; ++Index)
			{
				HashPtr[Index] = FSetElementId();
			}
		}
	}

	FORCEINLINE bool ShouldRehash(int32 NumHashedElements, int32 DesiredHashSize, bool bAllowShrinking = false) const
	{
		// If the hash hasn't been created yet, or is smaller than the desired hash size, rehash.
		// If shrinking is allowed and the hash is bigger than the desired hash size, rehash.
		return (
			(NumHashedElements > 0 && HashSize < DesiredHashSize) || (bAllowShrinking && HashSize > DesiredHashSize));
	}

	/** Resizes the hash when needed; returns true when it rehashed. */
	bool ConditionalRehash(int32 NumHashedElements, bool bAllowShrinking = false) const
	{
		// Calculate the desired hash size for the specified number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(NumHashedElements);

		if (ShouldRehash(NumHashedElements, DesiredHashSize, bAllowShrinking))
		{
			HashSize = DesiredHashSize;
			Rehash();
			return true;
		}

		return false;
	}

	/** Rebuilds the hash buckets from the elements. */
	void Rehash() const
	{
		// Free the old hash.
		Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));

		const int32 LocalHashSize = HashSize;
		if (LocalHashSize)
		{
			// Allocate the new hash.
			checkSlow(FMath::IsPowerOfTwo(HashSize));
			Hash.ResizeAllocation(0, LocalHashSize, sizeof(FSetElementId));
			for (int32 HashIndex = 0; HashIndex < LocalHashSize; ++HashIndex)
			{
				GetTypedHash(HashIndex) = FSetElementId();
			}

			// Add the existing elements to the new hash.
			for (typename ElementArrayType::TConstIterator ElementIt(Elements); ElementIt; ++ElementIt)
			{
				HashElement(FSetElementId(ElementIt.GetIndex()), *ElementIt);
			}
		}
	}

	ElementArrayType Elements;

	mutable HashType Hash;
	mutable int32 HashSize;
};

/** Offsets inside a TSetElement of a type-erased set (UE: FScriptSetLayout). */
struct FScriptSetLayout
{
	/** Offset of the element inside TSetElement (always 0). */
	int32 ElementOffset;
	int32 HashNextIdOffset;
	int32 HashIndexOffset;
	/** Size of a TSetElement. */
	int32 Size;
	FScriptSparseArrayLayout SparseArrayLayout;
};

/**
 * Untyped view of a TSet for the reflection system (UE: TScriptSet). Same layout as TSet<ElementType, KeyFuncs,
 * Allocator> for every ElementType. Hashing and equality come from the caller (the element property), and must be the
 * ones TSet uses (GetTypeHash and operator==), so native code and reflection can share a set. The callbacks are
 * template callables (UE passes TFunctionRef; Leon keeps Templates/Function.h out of the container headers).
 */
template <typename Allocator>
class TScriptSet
{
public:
	/** The TSetElement layout of a set of elements of this size and alignment. */
	static FScriptSetLayout GetScriptLayout(int32 ElementSize, int32 ElementAlignment)
	{
		FScriptSetLayout Result;
		const int32 Alignment = FMath::Max(ElementAlignment, int32(alignof(int32)));
		Result.ElementOffset = 0;
		Result.HashNextIdOffset = Align(ElementSize, alignof(FSetElementId));
		Result.HashIndexOffset = Result.HashNextIdOffset + int32(sizeof(FSetElementId));
		Result.Size = Align(Result.HashIndexOffset + int32(sizeof(int32)), Alignment);
		Result.SparseArrayLayout =
			TScriptSparseArray<typename Allocator::SparseArrayAllocator>::GetScriptLayout(Result.Size, Alignment);
		return Result;
	}

	TScriptSet()
		: HashSize(0)
	{
	}

	TScriptSet(const TScriptSet&) = delete;
	TScriptSet& operator=(const TScriptSet&) = delete;

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Elements.IsValidIndex(Index);
	}

	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Elements.GetMaxIndex();
	}

	FORCEINLINE void* GetData(int32 Index, const FScriptSetLayout& Layout)
	{
		return Elements.GetData(Index, Layout.SparseArrayLayout);
	}
	FORCEINLINE const void* GetData(int32 Index, const FScriptSetLayout& Layout) const
	{
		return Elements.GetData(Index, Layout.SparseArrayLayout);
	}

	/** Takes Other's elements; this set must hold no constructed elements. */
	void MoveAssign(TScriptSet& Other, const FScriptSetLayout& Layout)
	{
		checkSlow(this != &Other);
		Empty(0, Layout);
		Elements.MoveAssign(Other.Elements, Layout.SparseArrayLayout);
		Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));
		Hash.MoveToEmpty(Other.Hash);
		HashSize = Other.HashSize;
		Other.HashSize = 0;
	}

	/** Removes every element (already destroyed by the caller), keeping room for Slack. */
	void Empty(int32 Slack, const FScriptSetLayout& Layout)
	{
		Elements.Empty(Slack, Layout.SparseArrayLayout);
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(Slack);
		if (Slack != 0 && (HashSize == 0 || HashSize != DesiredHashSize))
		{
			HashSize = DesiredHashSize;
			Hash.ResizeAllocation(0, HashSize, sizeof(FSetElementId));
		}
		FSetElementId* HashPtr = (FSetElementId*)Hash.GetAllocation();
		for (int32 Index = 0; Index < HashSize; ++Index)
		{
			HashPtr[Index] = FSetElementId();
		}
	}

	/** Unlinks the element at Index from its bucket and frees its slot (the caller destroyed it). */
	void RemoveAt(int32 Index, const FScriptSetLayout& Layout)
	{
		check(IsValidIndex(Index));
		void* ElementBeingRemoved = Elements.GetData(Index, Layout.SparseArrayLayout);
		for (FSetElementId* NextElementId = &GetTypedHash(GetHashIndexRef(ElementBeingRemoved, Layout));
			NextElementId->IsValidId();
			NextElementId =
				&GetHashNextIdRef(Elements.GetData(NextElementId->AsInteger(), Layout.SparseArrayLayout), Layout))
		{
			if (NextElementId->AsInteger() == Index)
			{
				*NextElementId = GetHashNextIdRef(ElementBeingRemoved, Layout);
				break;
			}
		}
		Elements.RemoveAtUninitialized(Layout.SparseArrayLayout, Index);
	}

	/** Reserves an element slot; the set must be rehashed once the element is constructed. */
	int32 AddUninitialized(const FScriptSetLayout& Layout)
	{
		return Elements.AddUninitialized(Layout.SparseArrayLayout);
	}

	/** Rebuilds the hash buckets; GetKeyHash(const void* Element) hashes an element. */
	template <typename HashFnType>
	void Rehash(const FScriptSetLayout& Layout, HashFnType&& GetKeyHash)
	{
		Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));
		HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
		if (HashSize)
		{
			checkSlow(FMath::IsPowerOfTwo(HashSize));
			Hash.ResizeAllocation(0, HashSize, sizeof(FSetElementId));
			for (int32 HashIndex = 0; HashIndex < HashSize; ++HashIndex)
			{
				GetTypedHash(HashIndex) = FSetElementId();
			}
			for (int32 Index = 0, Count = Elements.Num(); Count; ++Index)
			{
				if (Elements.IsValidIndex(Index))
				{
					void* Element = Elements.GetData(Index, Layout.SparseArrayLayout);
					const int32 HashIndex = int32(GetKeyHash(Element) & uint32(HashSize - 1));
					GetHashIndexRef(Element, Layout) = HashIndex;
					GetHashNextIdRef(Element, Layout) = GetTypedHash(HashIndex);
					GetTypedHash(HashIndex) = FSetElementId::FromInteger(Index);
					--Count;
				}
			}
		}
	}

	/** Index of the element equal to Element, or INDEX_NONE (EqualityFn(Element, SetElement)). */
	template <typename HashFnType, typename EqualityFnType>
	int32 FindIndex(
		const void* Element, const FScriptSetLayout& Layout, HashFnType&& GetKeyHash, EqualityFnType&& EqualityFn) const
	{
		if (Elements.Num())
		{
			return FindIndexImpl(Element, Layout, GetKeyHash(Element), EqualityFn);
		}
		return INDEX_NONE;
	}

	/** Index of the element equal to Element; adds one built by ConstructFn(void* Slot) when there is none. */
	template <typename HashFnType, typename EqualityFnType, typename ConstructFnType>
	int32 FindOrAdd(const void* Element, const FScriptSetLayout& Layout, HashFnType&& GetKeyHash,
		EqualityFnType&& EqualityFn, ConstructFnType&& ConstructFn)
	{
		const uint32 KeyHash = GetKeyHash(Element);
		const int32 OldElementIndex = Elements.Num() ? FindIndexImpl(Element, Layout, KeyHash, EqualityFn) : INDEX_NONE;
		if (OldElementIndex != INDEX_NONE)
		{
			return OldElementIndex;
		}
		return AddNewElement(Layout, GetKeyHash, KeyHash, ConstructFn);
	}

	/** Adds Element (ConstructFn builds it in its slot), replacing an equal one as TSet::Add does. */
	template <typename HashFnType, typename EqualityFnType, typename ConstructFnType, typename DestructFnType>
	void Add(const void* Element, const FScriptSetLayout& Layout, HashFnType&& GetKeyHash, EqualityFnType&& EqualityFn,
		ConstructFnType&& ConstructFn, DestructFnType&& DestructFn)
	{
		const uint32 KeyHash = GetKeyHash(Element);
		const int32 OldElementIndex = Elements.Num() ? FindIndexImpl(Element, Layout, KeyHash, EqualityFn) : INDEX_NONE;
		if (OldElementIndex != INDEX_NONE)
		{
			void* ElementPtr = Elements.GetData(OldElementIndex, Layout.SparseArrayLayout);
			DestructFn(ElementPtr);
			ConstructFn(ElementPtr);
		}
		else
		{
			AddNewElement(Layout, GetKeyHash, KeyHash, ConstructFn);
		}
	}

	/** The script set must be a drop-in view of TSet (UE: TScriptSet::CheckConstraints). */
	static void CheckConstraints()
	{
		typedef TScriptSet ScriptType;
		typedef TSet<int32, DefaultKeyFuncs<int32>, Allocator> RealType;
		static_assert(sizeof(ScriptType) == sizeof(RealType), "TScriptSet's size doesn't match TSet");
		static_assert(alignof(ScriptType) == alignof(RealType), "TScriptSet's alignment doesn't match TSet");
		static_assert(offsetof(ScriptType, Elements) == offsetof(RealType, Elements),
			"TScriptSet's Elements offset doesn't match TSet");
		static_assert(
			offsetof(ScriptType, Hash) == offsetof(RealType, Hash), "TScriptSet's Hash offset doesn't match TSet");
		static_assert(offsetof(ScriptType, HashSize) == offsetof(RealType, HashSize),
			"TScriptSet's HashSize offset doesn't match TSet");
		static_assert(sizeof(FSetElementId) == sizeof(int32), "FSetElementId must be an int32");
		static_assert(sizeof(TSetElement<int32>) == 12, "TSetElement must be the value, HashNextId and HashIndex");
		TScriptSparseArray<typename Allocator::SparseArrayAllocator>::CheckConstraints();
	}

private:
	template <typename EqualityFnType>
	int32 FindIndexImpl(
		const void* Element, const FScriptSetLayout& Layout, uint32 KeyHash, EqualityFnType&& EqualityFn) const
	{
		const void* CurrentElement = nullptr;
		for (FSetElementId ElementId = GetTypedHash(int32(KeyHash)); ElementId.IsValidId();
			ElementId = GetHashNextIdRef(CurrentElement, Layout))
		{
			CurrentElement = Elements.GetData(ElementId.AsInteger(), Layout.SparseArrayLayout);
			if (EqualityFn(Element, CurrentElement))
			{
				return ElementId.AsInteger();
			}
		}
		return INDEX_NONE;
	}

	template <typename HashFnType, typename ConstructFnType>
	int32 AddNewElement(
		const FScriptSetLayout& Layout, HashFnType&& GetKeyHash, uint32 KeyHash, ConstructFnType&& ConstructFn)
	{
		const int32 NewElementIndex = Elements.AddUninitialized(Layout.SparseArrayLayout);
		void* ElementPtr = Elements.GetData(NewElementIndex, Layout.SparseArrayLayout);
		ConstructFn(ElementPtr);

		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(Num());
		if (!HashSize || HashSize < DesiredHashSize)
		{
			// The rehash links the new element too.
			Rehash(Layout, GetKeyHash);
		}
		else
		{
			const int32 HashIndex = int32(KeyHash & uint32(HashSize - 1));
			FSetElementId& TypedHash = GetTypedHash(HashIndex);
			GetHashIndexRef(ElementPtr, Layout) = HashIndex;
			GetHashNextIdRef(ElementPtr, Layout) = TypedHash;
			TypedHash = FSetElementId::FromInteger(NewElementIndex);
		}
		return NewElementIndex;
	}

	FORCEINLINE FSetElementId& GetTypedHash(int32 HashIndex) const
	{
		return ((FSetElementId*)Hash.GetAllocation())[HashIndex & (HashSize - 1)];
	}

	static FORCEINLINE FSetElementId& GetHashNextIdRef(const void* Element, const FScriptSetLayout& Layout)
	{
		return *(FSetElementId*)((uint8*)Element + Layout.HashNextIdOffset);
	}

	static FORCEINLINE int32& GetHashIndexRef(const void* Element, const FScriptSetLayout& Layout)
	{
		return *(int32*)((uint8*)Element + Layout.HashIndexOffset);
	}

	typedef TScriptSparseArray<typename Allocator::SparseArrayAllocator> ElementArrayType;
	typedef typename Allocator::HashAllocator::template ForElementType<FSetElementId> HashType;

	ElementArrayType Elements;
	mutable HashType Hash;
	mutable int32 HashSize;
};

/** Untyped TSet with the default allocator (UE: FScriptSet). */
class FScriptSet : public TScriptSet<FDefaultSetAllocator>
{
public:
	FScriptSet() = default;
};
