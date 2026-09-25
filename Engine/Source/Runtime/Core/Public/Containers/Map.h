#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Set.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Sorting.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>

/** Builds a TPair in place inside the map's set element (UE: TPairInitializer). */
template <typename KeyInitType, typename ValueInitType>
class TPairInitializer
{
public:
	typename TRValueToLValueReference<KeyInitType>::Type Key;
	typename TRValueToLValueReference<ValueInitType>::Type Value;

	FORCEINLINE TPairInitializer(KeyInitType InKey, ValueInitType InValue)
		: Key(InKey)
		, Value(InValue)
	{
	}

	/** Views a stored pair (the set asks its key functions for the key of stored elements). */
	template <typename KeyType, typename ValueType>
	FORCEINLINE TPairInitializer(const TPair<KeyType, ValueType>& Pair)
		: Key(Pair.Key)
		, Value(Pair.Value)
	{
	}

	template <typename KeyType, typename ValueType>
	operator TPair<KeyType, ValueType>() const
	{
		return TPair<KeyType, ValueType>(static_cast<KeyInitType>(Key), static_cast<ValueInitType>(Value));
	}
};

/** Builds a TPair from a key and a default value (UE: TKeyInitializer). */
template <typename KeyInitType>
class TKeyInitializer
{
public:
	typename TRValueToLValueReference<KeyInitType>::Type Key;

	FORCEINLINE explicit TKeyInitializer(KeyInitType InKey)
		: Key(InKey)
	{
	}

	template <typename KeyType, typename ValueType>
	operator TPair<KeyType, ValueType>() const
	{
		return TPair<KeyType, ValueType>(static_cast<KeyInitType>(Key), ValueType());
	}
};

/** Map key functions: the pair's Key is the set key (UE: TDefaultMapKeyFuncs). */
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys>
struct TDefaultMapKeyFuncs : BaseKeyFuncs<TPair<KeyType, ValueType>, KeyType, bInAllowDuplicateKeys>
{
	typedef typename TCallTraits<KeyType>::ParamType KeyInitType;
	typedef const TPairInitializer<typename TCallTraits<KeyType>::ParamType,
		typename TCallTraits<ValueType>::ParamType>& ElementInitType;

	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Key;
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

/** TDefaultMapKeyFuncs for hashable keys; the default of TMap / TMultiMap (UE: TDefaultMapHashableKeyFuncs). */
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys /* = false */>
struct TDefaultMapHashableKeyFuncs : TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>
{
};

/** Shared implementation of TMap / TMultiMap (UE: TMapBase). */
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
class TMapBase
{
	template <typename OtherKeyType, typename OtherValueType, typename OtherSetAllocator, typename OtherKeyFuncs>
	friend class TMapBase;
	// Checks that its layout matches (TScriptMap::CheckConstraints).
	template <typename>
	friend class TScriptMap;

public:
	typedef typename TCallTraits<KeyType>::ParamType KeyConstPointerType;
	typedef typename TCallTraits<KeyType>::ParamType KeyInitType;
	typedef typename TCallTraits<ValueType>::ParamType ValueInitType;
	typedef TPair<KeyType, ValueType> ElementType;

protected:
	TMapBase() = default;
	TMapBase(TMapBase&&) = default;
	TMapBase(const TMapBase&) = default;
	TMapBase& operator=(TMapBase&&) = default;
	TMapBase& operator=(const TMapBase&) = default;

public:
	/** Same key / value pairs in any order (UE: OrderIndependentCompareEqual). */
	bool OrderIndependentCompareEqual(const TMapBase& Other) const
	{
		// First check counts (they should be the same obviously).
		if (Num() != Other.Num())
		{
			return false;
		}

		// Since we know the counts are the same, we can just iterate one map and check for existence in the other.
		for (typename ElementSetType::TConstIterator It(Pairs); It; ++It)
		{
			const ValueType* BVal = Other.Find(It->Key);
			if (BVal == nullptr)
			{
				return false;
			}
			if (!(*BVal == It->Value))
			{
				return false;
			}
		}

		// Everything in A exists in B and they are the same size, so B's contents are the same as A.
		return true;
	}

	FORCEINLINE void Empty(int32 ExpectedNumElements = 0)
	{
		Pairs.Empty(ExpectedNumElements);
	}

	FORCEINLINE void Reset()
	{
		Pairs.Reset();
	}

	FORCEINLINE void Shrink()
	{
		Pairs.Shrink();
	}

	FORCEINLINE void Compact()
	{
		Pairs.Compact();
	}

	FORCEINLINE void CompactStable()
	{
		Pairs.CompactStable();
	}

	FORCEINLINE void Reserve(int32 Number)
	{
		Pairs.Reserve(Number);
	}

	FORCEINLINE int32 Num() const
	{
		return Pairs.Num();
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Pairs.IsEmpty();
	}

	/** Collects the unique keys; returns their number. */
	template <typename Allocator>
	int32 GetKeys(TArray<KeyType, Allocator>& OutKeys) const
	{
		OutKeys.Reset();

		TSet<KeyType> VisitedKeys;
		VisitedKeys.Reserve(Num());

		// Presize the array if we know there are supposed to be no duplicate keys.
		if (!KeyFuncs::bAllowDuplicateKeys)
		{
			OutKeys.Reserve(Num());
		}

		for (typename ElementSetType::TConstIterator It(Pairs); It; ++It)
		{
			// Even if bAllowDuplicateKeys is false, we still want to filter for duplicate keys due to maps with keys
			// that can be invalidated (UObjects, TWeakObj, etc.)
			if (!VisitedKeys.Contains(It->Key))
			{
				OutKeys.Add(It->Key);
				VisitedKeys.Add(It->Key);
			}
		}

		return OutKeys.Num();
	}

	SIZE_T GetAllocatedSize() const
	{
		return Pairs.GetAllocatedSize();
	}

	/** Sets (or replaces) the value of a key; returns a reference to the stored value. */
	FORCEINLINE ValueType& Add(const KeyType& InKey, const ValueType& InValue)
	{
		return Emplace(InKey, InValue);
	}
	FORCEINLINE ValueType& Add(const KeyType& InKey, ValueType&& InValue)
	{
		return Emplace(InKey, MoveTempIfPossible(InValue));
	}
	FORCEINLINE ValueType& Add(KeyType&& InKey, const ValueType& InValue)
	{
		return Emplace(MoveTempIfPossible(InKey), InValue);
	}
	FORCEINLINE ValueType& Add(KeyType&& InKey, ValueType&& InValue)
	{
		return Emplace(MoveTempIfPossible(InKey), MoveTempIfPossible(InValue));
	}

	/** Adds a key with a default-constructed value. */
	FORCEINLINE ValueType& Add(const KeyType& InKey)
	{
		return Emplace(InKey);
	}
	FORCEINLINE ValueType& Add(KeyType&& InKey)
	{
		return Emplace(MoveTempIfPossible(InKey));
	}

	FORCEINLINE ValueType& Add(const TTuple<KeyType, ValueType>& InKeyValue)
	{
		return Emplace(InKeyValue.Key, InKeyValue.Value);
	}
	FORCEINLINE ValueType& Add(TTuple<KeyType, ValueType>&& InKeyValue)
	{
		return Emplace(MoveTempIfPossible(InKeyValue.Key), MoveTempIfPossible(InKeyValue.Value));
	}

	template <typename InitKeyType, typename InitValueType>
	ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue)
	{
		const FSetElementId PairId = Pairs.Emplace(TPairInitializer<InitKeyType&&, InitValueType&&>(
			Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));
		return Pairs[PairId].Value;
	}

	template <typename InitKeyType>
	ValueType& Emplace(InitKeyType&& InKey)
	{
		const FSetElementId PairId = Pairs.Emplace(TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));
		return Pairs[PairId].Value;
	}

	/** Removes every pair with the key; returns how many were removed. */
	FORCEINLINE int32 Remove(KeyConstPointerType InKey)
	{
		return Pairs.Remove(InKey);
	}

	/** First key mapped to Value (linear search), or nullptr. */
	const KeyType* FindKey(ValueInitType Value) const
	{
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			if (PairIt->Value == Value)
			{
				return &PairIt->Key;
			}
		}
		return nullptr;
	}

	/** Pairs matching a predicate on the pair. */
	template <typename Predicate>
	TMap<KeyType, ValueType> FilterByPredicate(Predicate Pred) const
	{
		TMap<KeyType, ValueType> FilterResults;
		FilterResults.Reserve(Pairs.Num());
		for (const ElementType& Pair : Pairs)
		{
			if (Pred(Pair))
			{
				FilterResults.Add(Pair);
			}
		}
		return FilterResults;
	}

	FORCEINLINE ValueType* Find(KeyConstPointerType Key)
	{
		if (auto* Pair = Pairs.Find(Key))
		{
			return &Pair->Value;
		}
		return nullptr;
	}
	FORCEINLINE const ValueType* Find(KeyConstPointerType Key) const
	{
		return const_cast<TMapBase*>(this)->Find(Key);
	}

	/** Value of the key, adding a default-constructed one when missing. */
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key)
	{
		return FindOrAddImpl(Key);
	}
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key)
	{
		return FindOrAddImpl(MoveTempIfPossible(Key));
	}

	/** Value of the key, adding Value when missing. */
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key, const ValueType& Value)
	{
		return FindOrAddImpl(Key, Value);
	}
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key, ValueType&& Value)
	{
		return FindOrAddImpl(Key, MoveTempIfPossible(Value));
	}
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key, const ValueType& Value)
	{
		return FindOrAddImpl(MoveTempIfPossible(Key), Value);
	}
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key, ValueType&& Value)
	{
		return FindOrAddImpl(MoveTempIfPossible(Key), MoveTempIfPossible(Value));
	}

	/** Value of an existing key (fatal error when missing). */
	FORCEINLINE const ValueType& FindChecked(KeyConstPointerType Key) const
	{
		const auto* Pair = Pairs.Find(Key);
		check(Pair != nullptr);
		return Pair->Value;
	}
	FORCEINLINE ValueType& FindChecked(KeyConstPointerType Key)
	{
		auto* Pair = Pairs.Find(Key);
		check(Pair != nullptr);
		return Pair->Value;
	}

	/** Copy of the value, or a default-constructed value when missing. */
	FORCEINLINE ValueType FindRef(KeyConstPointerType Key) const
	{
		if (const auto* Pair = Pairs.Find(Key))
		{
			return Pair->Value;
		}
		return ValueType();
	}

	FORCEINLINE bool Contains(KeyConstPointerType Key) const
	{
		return Pairs.Contains(Key);
	}

	template <typename Allocator>
	void GenerateKeyArray(TArray<KeyType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			OutArray.Add(PairIt->Key);
		}
	}

	template <typename Allocator>
	void GenerateValueArray(TArray<ValueType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			OutArray.Add(PairIt->Value);
		}
	}

protected:
	typedef TSet<ElementType, KeyFuncs, SetAllocator> ElementSetType;

	template <typename ArgType>
	FORCEINLINE ValueType& FindOrAddImpl(ArgType&& Arg)
	{
		if (auto* Pair = Pairs.Find(Arg))
		{
			return Pair->Value;
		}
		return Add(Forward<ArgType>(Arg));
	}

	template <typename InitKeyType, typename InitValueType>
	ValueType& FindOrAddImpl(InitKeyType&& Key, InitValueType&& Value)
	{
		if (auto* Pair = Pairs.Find(Key))
		{
			return Pair->Value;
		}
		return Add(Forward<InitKeyType>(Key), Forward<InitValueType>(Value));
	}

	/** Iterator over the pairs (UE: TMapBase::TBaseIterator). */
	template <bool bConst, bool bRangedFor = false>
	class TBaseIterator
	{
	public:
		typedef std::conditional_t<bConst, typename ElementSetType::TConstIterator, typename ElementSetType::TIterator>
			PairItType;

	private:
		typedef std::conditional_t<bConst, const TMapBase, TMapBase> MapType;
		typedef std::conditional_t<bConst, const KeyType, KeyType> ItKeyType;
		typedef std::conditional_t<bConst, const ValueType, ValueType> ItValueType;
		typedef std::conditional_t<bConst, const typename ElementSetType::ElementType,
			typename ElementSetType::ElementType>
			PairType;

	public:
		FORCEINLINE TBaseIterator(const PairItType& InElementIt)
			: PairIt(InElementIt)
		{
		}

		FORCEINLINE TBaseIterator& operator++()
		{
			++PairIt;
			return *this;
		}

		FORCEINLINE explicit operator bool() const
		{
			return !!PairIt;
		}

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs)
		{
			return Lhs.PairIt == Rhs.PairIt;
		}
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs)
		{
			return Lhs.PairIt != Rhs.PairIt;
		}

		FORCEINLINE ItKeyType& Key() const
		{
			return PairIt->Key;
		}
		FORCEINLINE ItValueType& Value() const
		{
			return PairIt->Value;
		}

		FORCEINLINE PairType& operator*() const
		{
			return *PairIt;
		}
		FORCEINLINE PairType* operator->() const
		{
			return &*PairIt;
		}

	protected:
		PairItType PairIt;
	};

	/** Iterates the pairs with a given key (UE: TMapBase::TBaseKeyIterator). */
	template <bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef std::conditional_t<bConst, typename ElementSetType::TConstKeyIterator,
			typename ElementSetType::TKeyIterator>
			SetItType;
		typedef std::conditional_t<bConst, const KeyType, KeyType> ItKeyType;
		typedef std::conditional_t<bConst, const ValueType, ValueType> ItValueType;

	public:
		FORCEINLINE TBaseKeyIterator(const SetItType& InSetIt)
			: SetIt(InSetIt)
		{
		}

		FORCEINLINE TBaseKeyIterator& operator++()
		{
			++SetIt;
			return *this;
		}

		FORCEINLINE explicit operator bool() const
		{
			return !!SetIt;
		}

		FORCEINLINE ItKeyType& Key() const
		{
			return SetIt->Key;
		}
		FORCEINLINE ItValueType& Value() const
		{
			return SetIt->Value;
		}

	protected:
		SetItType SetIt;
	};

	ElementSetType Pairs;

public:
	class TIterator : public TBaseIterator<false>
	{
	public:
		FORCEINLINE TIterator(TMapBase& InMap, bool bInRequiresRehashOnRemoval = false)
			: TBaseIterator<false>(InMap.Pairs.CreateIterator())
			, Map(InMap)
			, bElementsHaveBeenRemoved(false)
			, bRequiresRehashOnRemoval(bInRequiresRehashOnRemoval)
		{
		}

		FORCEINLINE ~TIterator()
		{
			if (bElementsHaveBeenRemoved && bRequiresRehashOnRemoval)
			{
				Map.Pairs.Relax();
			}
		}

		/** Removes the current pair; iteration stays valid. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseIterator<false>::PairIt.RemoveCurrent();
			bElementsHaveBeenRemoved = true;
		}

	private:
		TMapBase& Map;
		bool bElementsHaveBeenRemoved;
		bool bRequiresRehashOnRemoval;
	};

	class TConstIterator : public TBaseIterator<true>
	{
	public:
		FORCEINLINE TConstIterator(const TMapBase& InMap)
			: TBaseIterator<true>(InMap.Pairs.CreateConstIterator())
		{
		}
	};

	using TRangedForIterator = TBaseIterator<false, true>;
	using TRangedForConstIterator = TBaseIterator<true, true>;

	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	public:
		FORCEINLINE TConstKeyIterator(const TMapBase& InMap, KeyInitType InKey)
			: TBaseKeyIterator<true>(typename ElementSetType::TConstKeyIterator(InMap.Pairs, InKey))
		{
		}
	};

	class TKeyIterator : public TBaseKeyIterator<false>
	{
	public:
		FORCEINLINE TKeyIterator(TMapBase& InMap, KeyInitType InKey)
			: TBaseKeyIterator<false>(typename ElementSetType::TKeyIterator(InMap.Pairs, InKey))
		{
		}

		/** Removes the current pair; iteration stays valid. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseKeyIterator<false>::SetIt.RemoveCurrent();
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

	FORCEINLINE TKeyIterator CreateKeyIterator(KeyInitType InKey)
	{
		return TKeyIterator(*this, InKey);
	}

	FORCEINLINE TConstKeyIterator CreateConstKeyIterator(KeyInitType InKey) const
	{
		return TConstKeyIterator(*this, InKey);
	}

	// Ranged-for support (lower-case names required by the language).
	FORCEINLINE TRangedForIterator begin()
	{
		return TRangedForIterator(Pairs.begin());
	}
	FORCEINLINE TRangedForConstIterator begin() const
	{
		return TRangedForConstIterator(Pairs.begin());
	}
	FORCEINLINE TRangedForIterator end()
	{
		return TRangedForIterator(Pairs.end());
	}
	FORCEINLINE TRangedForConstIterator end() const
	{
		return TRangedForConstIterator(Pairs.end());
	}
};

/** TMapBase with sorting (UE: TSortableMapBase). */
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
class TSortableMapBase : public TMapBase<KeyType, ValueType, SetAllocator, KeyFuncs>
{
protected:
	typedef TMapBase<KeyType, ValueType, SetAllocator, KeyFuncs> Super;

	TSortableMapBase() = default;
	TSortableMapBase(TSortableMapBase&&) = default;
	TSortableMapBase(const TSortableMapBase&) = default;
	TSortableMapBase& operator=(TSortableMapBase&&) = default;
	TSortableMapBase& operator=(const TSortableMapBase&) = default;

public:
	/** Sorts the pairs by key (unstable). */
	template <typename PredicateType>
	FORCEINLINE void KeySort(const PredicateType& Predicate)
	{
		Super::Pairs.Sort(FKeyComparisonClass<PredicateType>(Predicate));
	}

	template <typename PredicateType>
	FORCEINLINE void KeyStableSort(const PredicateType& Predicate)
	{
		Super::Pairs.StableSort(FKeyComparisonClass<PredicateType>(Predicate));
	}

	/** Sorts the pairs by value (unstable). */
	template <typename PredicateType>
	FORCEINLINE void ValueSort(const PredicateType& Predicate)
	{
		Super::Pairs.Sort(FValueComparisonClass<PredicateType>(Predicate));
	}

	template <typename PredicateType>
	FORCEINLINE void ValueStableSort(const PredicateType& Predicate)
	{
		Super::Pairs.StableSort(FValueComparisonClass<PredicateType>(Predicate));
	}

	/** Sorts the free list so new pairs fill the lowest holes first. */
	void SortFreeList()
	{
		Super::Pairs.CompactStable();
	}

private:
	template <typename PredicateType>
	class FKeyComparisonClass
	{
		TDereferenceWrapper<KeyType, PredicateType> Predicate;

	public:
		FORCEINLINE FKeyComparisonClass(const PredicateType& InPredicate)
			: Predicate(InPredicate)
		{
		}

		FORCEINLINE bool operator()(const typename Super::ElementType& A, const typename Super::ElementType& B) const
		{
			return Predicate(A.Key, B.Key);
		}
	};

	template <typename PredicateType>
	class FValueComparisonClass
	{
		TDereferenceWrapper<ValueType, PredicateType> Predicate;

	public:
		FORCEINLINE FValueComparisonClass(const PredicateType& InPredicate)
			: Predicate(InPredicate)
		{
		}

		FORCEINLINE bool operator()(const typename Super::ElementType& A, const typename Super::ElementType& B) const
		{
			return Predicate(A.Value, B.Value);
		}
	};
};

/** Hash map with unique keys (UE: TMap). Iteration yields TPair<KeyType, ValueType>&. */
template <typename InKeyType, typename InValueType, typename SetAllocator /* = FDefaultSetAllocator */,
	typename KeyFuncs /* = TDefaultMapHashableKeyFuncs<KeyType, ValueType, false> */>
class TMap : public TSortableMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>
{
	static_assert(
		!KeyFuncs::bAllowDuplicateKeys, "TMap cannot be instantiated with a KeyFuncs which allows duplicate keys");

public:
	typedef InKeyType KeyType;
	typedef InValueType ValueType;
	typedef SetAllocator SetAllocatorType;
	typedef KeyFuncs KeyFuncsType;

	typedef TSortableMapBase<KeyType, ValueType, SetAllocator, KeyFuncs> Super;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::KeyConstPointerType KeyConstPointerType;

	TMap() = default;
	TMap(TMap&&) = default;
	TMap(const TMap&) = default;
	TMap& operator=(TMap&&) = default;
	TMap& operator=(const TMap&) = default;

	TMap(std::initializer_list<TPairInitializer<const KeyType&, const ValueType&>> InitList)
	{
		this->Reserve(int32(InitList.size()));
		for (const TPairInitializer<const KeyType&, const ValueType&>& Element : InitList)
		{
			this->Add(Element.Key, Element.Value);
		}
	}

	TMap& operator=(std::initializer_list<TPairInitializer<const KeyType&, const ValueType&>> InitList)
	{
		this->Empty(int32(InitList.size()));
		for (const TPairInitializer<const KeyType&, const ValueType&>& Element : InitList)
		{
			this->Add(Element.Key, Element.Value);
		}
		return *this;
	}

	/** Removes the key and copies its value out; false when missing. */
	FORCEINLINE bool RemoveAndCopyValue(KeyInitType Key, ValueType& OutRemovedValue)
	{
		const FSetElementId PairId = Super::Pairs.FindId(Key);
		if (!PairId.IsValidId())
		{
			return false;
		}

		OutRemovedValue = MoveTempIfPossible(Super::Pairs[PairId].Value);
		Super::Pairs.Remove(PairId);
		return true;
	}

	/** Removes an existing key and returns its value (fatal error when missing). */
	FORCEINLINE ValueType FindAndRemoveChecked(KeyConstPointerType Key)
	{
		const FSetElementId PairId = Super::Pairs.FindId(Key);
		check(PairId.IsValidId());
		ValueType Result = MoveTempIfPossible(Super::Pairs[PairId].Value);
		Super::Pairs.Remove(PairId);
		return Result;
	}

	/** Adds every pair of Other, replacing the values of existing keys. */
	template <typename OtherSetAllocator>
	void Append(TMap<KeyType, ValueType, OtherSetAllocator, KeyFuncs>&& OtherMap)
	{
		this->Reserve(this->Num() + OtherMap.Num());
		for (auto& Pair : OtherMap)
		{
			this->Add(MoveTempIfPossible(Pair.Key), MoveTempIfPossible(Pair.Value));
		}
		OtherMap.Reset();
	}

	template <typename OtherSetAllocator>
	void Append(const TMap<KeyType, ValueType, OtherSetAllocator, KeyFuncs>& OtherMap)
	{
		this->Reserve(this->Num() + OtherMap.Num());
		for (const auto& Pair : OtherMap)
		{
			this->Add(Pair.Key, Pair.Value);
		}
	}

	FORCEINLINE ValueType& operator[](KeyConstPointerType Key)
	{
		return this->FindChecked(Key);
	}
	FORCEINLINE const ValueType& operator[](KeyConstPointerType Key) const
	{
		return this->FindChecked(Key);
	}
};

/** Hash map allowing several values per key (UE: TMultiMap). */
template <typename KeyType, typename ValueType, typename SetAllocator /* = FDefaultSetAllocator */,
	typename KeyFuncs /* = TDefaultMapHashableKeyFuncs<KeyType, ValueType, true> */>
class TMultiMap : public TSortableMapBase<KeyType, ValueType, SetAllocator, KeyFuncs>
{
	static_assert(KeyFuncs::bAllowDuplicateKeys,
		"TMultiMap cannot be instantiated with a KeyFuncs which disallows duplicate keys");

public:
	typedef TSortableMapBase<KeyType, ValueType, SetAllocator, KeyFuncs> Super;
	typedef typename Super::KeyConstPointerType KeyConstPointerType;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::ValueInitType ValueInitType;

	TMultiMap() = default;
	TMultiMap(TMultiMap&&) = default;
	TMultiMap(const TMultiMap&) = default;
	TMultiMap& operator=(TMultiMap&&) = default;
	TMultiMap& operator=(const TMultiMap&) = default;

	TMultiMap(std::initializer_list<TPairInitializer<const KeyType&, const ValueType&>> InitList)
	{
		this->Reserve(int32(InitList.size()));
		for (const TPairInitializer<const KeyType&, const ValueType&>& Element : InitList)
		{
			this->Add(Element.Key, Element.Value);
		}
	}

	/** Collects every value of the key (optionally in insertion order). */
	template <typename Allocator>
	void MultiFind(KeyInitType Key, TArray<ValueType, Allocator>& OutValues, bool bMaintainOrder = false) const
	{
		for (typename Super::ElementSetType::TConstKeyIterator It(Super::Pairs, Key); It; ++It)
		{
			OutValues.Add(It->Value);
		}

		if (bMaintainOrder)
		{
			// The hash chain visits the pairs from the newest; reverse for insertion order.
			for (int32 Index = 0, Last = OutValues.Num() - 1; Index < Last; ++Index, --Last)
			{
				OutValues.Swap(Index, Last);
			}
		}
	}

	/** Pointers to every value of the key. */
	template <typename Allocator>
	void MultiFindPointer(
		KeyInitType Key, TArray<const ValueType*, Allocator>& OutValues, bool bMaintainOrder = false) const
	{
		for (typename Super::ElementSetType::TConstKeyIterator It(Super::Pairs, Key); It; ++It)
		{
			OutValues.Add(&It->Value);
		}

		if (bMaintainOrder)
		{
			for (int32 Index = 0, Last = OutValues.Num() - 1; Index < Last; ++Index, --Last)
			{
				OutValues.Swap(Index, Last);
			}
		}
	}

	/** Adds the pair unless the same key / value pair exists; returns the stored value. */
	FORCEINLINE ValueType& AddUnique(const KeyType& InKey, const ValueType& InValue)
	{
		return EmplaceUnique(InKey, InValue);
	}
	FORCEINLINE ValueType& AddUnique(KeyType&& InKey, ValueType&& InValue)
	{
		return EmplaceUnique(MoveTempIfPossible(InKey), MoveTempIfPossible(InValue));
	}

	template <typename InitKeyType, typename InitValueType>
	ValueType& EmplaceUnique(InitKeyType&& InKey, InitValueType&& InValue)
	{
		if (ValueType* Found = FindPair(InKey, InValue))
		{
			return *Found;
		}

		// If there's no existing association with the same key and value, create one.
		return Super::Emplace(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue));
	}

	using Super::Remove;

	/** Removes every pair with the key and value; returns how many were removed. */
	FORCEINLINE int32 Remove(KeyInitType InKey, ValueInitType InValue)
	{
		// Iterate over pairs with a matching key.
		int32 NumRemovedPairs = 0;
		for (typename Super::ElementSetType::TKeyIterator It(Super::Pairs, InKey); It; ++It)
		{
			// If this pair has a matching value as well, remove it.
			if (It->Value == InValue)
			{
				It.RemoveCurrent();
				++NumRemovedPairs;
			}
		}
		return NumRemovedPairs;
	}

	/** Removes the first pair with the key and value; returns 0 or 1. */
	FORCEINLINE int32 RemoveSingle(KeyInitType InKey, ValueInitType InValue)
	{
		for (typename Super::ElementSetType::TKeyIterator It(Super::Pairs, InKey); It; ++It)
		{
			if (It->Value == InValue)
			{
				It.RemoveCurrent();
				return 1;
			}
		}
		return 0;
	}

	/** The stored value of the key / value pair, or nullptr. */
	FORCEINLINE const ValueType* FindPair(KeyInitType Key, ValueInitType Value) const
	{
		return const_cast<TMultiMap*>(this)->FindPair(Key, Value);
	}

	ValueType* FindPair(KeyInitType Key, ValueInitType Value)
	{
		for (typename Super::ElementSetType::TKeyIterator It(Super::Pairs, Key); It; ++It)
		{
			if (It->Value == Value)
			{
				return &It->Value;
			}
		}
		return nullptr;
	}

	/** Number of values of the key. */
	int32 Num(KeyInitType Key) const
	{
		int32 NumMatchingPairs = 0;
		for (typename Super::ElementSetType::TConstKeyIterator It(Super::Pairs, Key); It; ++It)
		{
			++NumMatchingPairs;
		}
		return NumMatchingPairs;
	}

	using Super::Num;
};

template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
struct TIsZeroConstructType<TMap<KeyType, ValueType, SetAllocator, KeyFuncs>>
{
	enum
	{
		Value = false
	};
};

/** Key / value offsets inside a TPair of a type-erased map (UE: FScriptMapLayout). */
struct FScriptMapLayout
{
	/** The key is at offset 0 of the pair; the value follows it, aligned. */
	int32 ValueOffset;
	FScriptSetLayout SetLayout;
};

/**
 * Untyped view of a TMap for the reflection system (UE: TScriptMap): a TScriptSet of TPair<Key, Value>. Same layout
 * as TMap<Key, Value, Allocator> for every key and value type; hashing and equality apply to the key only.
 */
template <typename AllocatorType>
class TScriptMap
{
public:
	/** The pair and set-element layout of a map with these key and value types. */
	static FScriptMapLayout GetScriptLayout(int32 KeySize, int32 KeyAlignment, int32 ValueSize, int32 ValueAlignment)
	{
		FScriptMapLayout Result;
		Result.ValueOffset = Align(KeySize, ValueAlignment);
		const int32 PairAlignment = FMath::Max(KeyAlignment, ValueAlignment);
		const int32 PairSize = Align(Result.ValueOffset + ValueSize, PairAlignment);
		Result.SetLayout = TScriptSet<AllocatorType>::GetScriptLayout(PairSize, PairAlignment);
		return Result;
	}

	TScriptMap() = default;
	TScriptMap(const TScriptMap&) = delete;
	TScriptMap& operator=(const TScriptMap&) = delete;

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Pairs.IsValidIndex(Index);
	}

	FORCEINLINE int32 Num() const
	{
		return Pairs.Num();
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Pairs.GetMaxIndex();
	}

	FORCEINLINE void* GetData(int32 Index, const FScriptMapLayout& Layout)
	{
		return Pairs.GetData(Index, Layout.SetLayout);
	}
	FORCEINLINE const void* GetData(int32 Index, const FScriptMapLayout& Layout) const
	{
		return Pairs.GetData(Index, Layout.SetLayout);
	}

	void MoveAssign(TScriptMap& Other, const FScriptMapLayout& Layout)
	{
		Pairs.MoveAssign(Other.Pairs, Layout.SetLayout);
	}

	void Empty(int32 Slack, const FScriptMapLayout& Layout)
	{
		Pairs.Empty(Slack, Layout.SetLayout);
	}

	void RemoveAt(int32 Index, const FScriptMapLayout& Layout)
	{
		Pairs.RemoveAt(Index, Layout.SetLayout);
	}

	int32 AddUninitialized(const FScriptMapLayout& Layout)
	{
		return Pairs.AddUninitialized(Layout.SetLayout);
	}

	/** Rebuilds the hash; GetKeyHash hashes a key (the start of a pair). */
	template <typename HashFnType>
	void Rehash(const FScriptMapLayout& Layout, HashFnType&& GetKeyHash)
	{
		Pairs.Rehash(Layout.SetLayout, GetKeyHash);
	}

	/** Index of the pair whose key equals Key, or INDEX_NONE (the key is at the start of a pair). */
	template <typename HashFnType, typename EqualityFnType>
	int32 FindPairIndex(
		const void* Key, const FScriptMapLayout& Layout, HashFnType&& GetKeyHash, EqualityFnType&& KeyEqualityFn) const
	{
		return Pairs.FindIndex(Key, Layout.SetLayout, GetKeyHash, KeyEqualityFn);
	}

	/** The value of the pair whose key equals Key, or nullptr. */
	template <typename HashFnType, typename EqualityFnType>
	uint8* FindValue(
		const void* Key, const FScriptMapLayout& Layout, HashFnType&& GetKeyHash, EqualityFnType&& KeyEqualityFn)
	{
		const int32 FoundIndex = FindPairIndex(Key, Layout, GetKeyHash, KeyEqualityFn);
		return FoundIndex != INDEX_NONE ? (uint8*)GetData(FoundIndex, Layout) + Layout.ValueOffset : nullptr;
	}

	/** The pair of Key, added with ConstructPairFn when missing; returns its index. */
	template <typename HashFnType, typename EqualityFnType, typename ConstructFnType>
	int32 FindOrAdd(const void* Key, const FScriptMapLayout& Layout, HashFnType&& GetKeyHash,
		EqualityFnType&& KeyEqualityFn, ConstructFnType&& ConstructPairFn)
	{
		return Pairs.FindOrAdd(Key, Layout.SetLayout, GetKeyHash, KeyEqualityFn, ConstructPairFn);
	}

	/** The script map must be a drop-in view of TMap (UE: TScriptMap::CheckConstraints). */
	static void CheckConstraints()
	{
		typedef TScriptMap ScriptType;
		typedef TMap<int32, int8, AllocatorType> RealType;
		static_assert(sizeof(ScriptType) == sizeof(RealType), "TScriptMap's size doesn't match TMap");
		static_assert(alignof(ScriptType) == alignof(RealType), "TScriptMap's alignment doesn't match TMap");
		static_assert(sizeof(TPair<int32, int8>) == 8, "TPair must be the key then the value");
		TScriptSet<AllocatorType>::CheckConstraints();
	}

private:
	TScriptSet<AllocatorType> Pairs;
};

/** Untyped TMap with the default allocator (UE: FScriptMap). */
class FScriptMap : public TScriptMap<FDefaultSetAllocator>
{
public:
	FScriptMap() = default;
};
