#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

#include <cstddef>
#include <type_traits>

// Dynamic array of bits stored in 32-bit words (UE: Containers/BitArray.h). Invariant: the bits past Num() in the
// last word are zero, so word scans never see stale bits.

enum
{
	NumBitsPerDWORD = 32,
	NumBitsPerDWORDLogTwo = 5
};

/** Mutable reference to a bit (UE: FBitReference). */
class FBitReference
{
public:
	FORCEINLINE FBitReference(uint32& InData, uint32 InMask)
		: Data(InData)
		, Mask(InMask)
	{
	}

	FORCEINLINE operator bool() const
	{
		return (Data & Mask) != 0;
	}

	FORCEINLINE void operator=(const bool NewValue)
	{
		if (NewValue)
		{
			Data |= Mask;
		}
		else
		{
			Data &= ~Mask;
		}
	}

	FORCEINLINE void operator|=(const bool NewValue)
	{
		if (NewValue)
		{
			Data |= Mask;
		}
	}

	FORCEINLINE void operator&=(const bool NewValue)
	{
		if (!NewValue)
		{
			Data &= ~Mask;
		}
	}

	FORCEINLINE FBitReference& operator=(const FBitReference& Copy)
	{
		// Assigns the referenced bit, not the reference.
		*this = (bool)Copy;
		return *this;
	}

private:
	uint32& Data;
	uint32 Mask;
};

/** Read-only reference to a bit (UE: FConstBitReference). */
class FConstBitReference
{
public:
	FORCEINLINE FConstBitReference(const uint32& InData, uint32 InMask)
		: Data(InData)
		, Mask(InMask)
	{
	}

	FORCEINLINE operator bool() const
	{
		return (Data & Mask) != 0;
	}

private:
	const uint32& Data;
	uint32 Mask;
};

/** Word index + mask of a bit, reusable across bit arrays of the same layout (UE: FRelativeBitReference). */
class FRelativeBitReference
{
public:
	FORCEINLINE explicit FRelativeBitReference(int32 BitIndex)
		: DWORDIndex(BitIndex >> NumBitsPerDWORDLogTwo)
		, Mask(1u << (BitIndex & (NumBitsPerDWORD - 1)))
	{
	}

	int32 DWORDIndex;
	uint32 Mask;
};

template <typename Allocator /* = FDefaultBitArrayAllocator */>
class TBitArray
{
	typedef uint32 WordType;
	typedef typename Allocator::template ForElementType<WordType> AllocatorType;

	template <typename>
	friend class TConstSetBitIterator;
	// Checks that its layout matches (TScriptBitArray::CheckConstraints).
	template <typename>
	friend class TScriptBitArray;

public:
	TBitArray()
		: NumBits(0)
		, MaxBits(AllocatorInstance.GetInitialCapacity() * NumBitsPerDWORD)
	{
		// Inline storage starts as garbage; the invariant wants zero words.
		ClearWordsFrom(0);
	}

	FORCEINLINE explicit TBitArray(bool bValue, int32 InNumBits)
		: TBitArray()
	{
		Init(bValue, InNumBits);
	}

	FORCEINLINE TBitArray(const TBitArray& Copy)
		: TBitArray()
	{
		*this = Copy;
	}

	FORCEINLINE TBitArray(TBitArray&& Other)
		: TBitArray()
	{
		MoveOrCopy(*this, Other);
	}

	FORCEINLINE TBitArray& operator=(const TBitArray& Copy)
	{
		if (this != &Copy)
		{
			Empty(Copy.Num());
			NumBits = Copy.NumBits;
			if (NumBits)
			{
				FMemory::Memcpy(GetData(), Copy.GetData(), GetNumWords() * sizeof(WordType));
			}
		}
		return *this;
	}

	FORCEINLINE TBitArray& operator=(TBitArray&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}
		return *this;
	}

	bool operator==(const TBitArray& Other) const
	{
		if (Num() != Other.Num())
		{
			return false;
		}
		return FMemory::Memcmp(GetData(), Other.GetData(), GetNumWords() * sizeof(WordType)) == 0;
	}

	FORCEINLINE bool operator!=(const TBitArray& Other) const
	{
		return !(*this == Other);
	}

	/** Appends a bit; returns its index. */
	int32 Add(const bool bValue)
	{
		const int32 Index = AddUninitialized(1);
		SetBitNoCheck(Index, bValue);
		return Index;
	}

	/** Appends NumBitsToAdd copies of a bit; returns the index of the first. */
	int32 Add(const bool bValue, int32 NumBitsToAdd)
	{
		const int32 Index = AddUninitialized(NumBitsToAdd);
		if (bValue && NumBitsToAdd)
		{
			SetRange(Index, NumBitsToAdd, true);
		}
		return Index;
	}

	/** Appends NumBitsToAdd zero bits (the invariant keeps unused bits zero); returns the index of the first. */
	int32 AddUninitialized(int32 NumBitsToAdd)
	{
		check(NumBitsToAdd >= 0);
		const int32 AddedIndex = NumBits;
		if (NumBitsToAdd > 0)
		{
			const int32 OldNumWords = GetNumWords();
			NumBits += NumBitsToAdd;
			if (NumBits > MaxBits)
			{
				const int32 MaxDWORDs =
					AllocatorInstance.CalculateSlackGrow(FMath::DivideAndRoundUp(NumBits, int32(NumBitsPerDWORD)),
						MaxBits / NumBitsPerDWORD, sizeof(WordType));
				Realloc(OldNumWords, MaxDWORDs);
			}
			const int32 NewNumWords = GetNumWords();
			if (NewNumWords > OldNumWords)
			{
				FMemory::Memzero(GetData() + OldNumWords, (NewNumWords - OldNumWords) * sizeof(WordType));
			}
		}
		return AddedIndex;
	}

	/** Removes every bit; the allocation shrinks / grows to ExpectedNumBits. */
	void Empty(int32 ExpectedNumBits = 0)
	{
		ExpectedNumBits = FMath::DivideAndRoundUp(ExpectedNumBits, int32(NumBitsPerDWORD)) * NumBitsPerDWORD;
		const int32 InitialMaxBits = AllocatorInstance.GetInitialCapacity() * NumBitsPerDWORD;
		NumBits = 0;
		// Grow when more bits are expected, shrink back to the initial capacity otherwise; else reuse the words.
		if (ExpectedNumBits > MaxBits || MaxBits > InitialMaxBits)
		{
			Realloc(0, FMath::Max(ExpectedNumBits, InitialMaxBits) / NumBitsPerDWORD);
		}
		else
		{
			ClearWordsFrom(0);
		}
	}

	void Reserve(int32 Number)
	{
		if (Number > MaxBits)
		{
			const int32 MaxDWORDs = AllocatorInstance.CalculateSlackReserve(
				FMath::DivideAndRoundUp(Number, int32(NumBitsPerDWORD)), sizeof(WordType));
			Realloc(GetNumWords(), MaxDWORDs);
		}
	}

	/** Removes every bit, keeping the allocation. */
	void Reset()
	{
		FMemory::Memzero(GetData(), GetNumWords() * sizeof(WordType));
		NumBits = 0;
	}

	/** Resets the array to NumBits copies of bValue. */
	FORCEINLINE void Init(bool bValue, int32 InNumBits)
	{
		Empty(InNumBits);
		if (InNumBits)
		{
			NumBits = InNumBits;
			FMemory::Memset(GetData(), bValue ? 0xff : 0, GetNumWords() * sizeof(WordType));
			ClearPartialSlackBits();
		}
	}

	/** Grows with bits set to bValue or shrinks to InNumBits. */
	void SetNum(int32 InNumBits, bool bValue)
	{
		if (InNumBits > NumBits)
		{
			Add(bValue, InNumBits - NumBits);
		}
		else if (InNumBits < NumBits)
		{
			RemoveAt(InNumBits, NumBits - InNumBits);
		}
	}

	/** Sets NumBitsToSet bits starting at Index to bValue. */
	void SetRange(int32 Index, int32 NumBitsToSet, bool bValue)
	{
		check(Index >= 0 && NumBitsToSet >= 0 && Index + NumBitsToSet <= NumBits);
		for (int32 BitIndex = Index, End = Index + NumBitsToSet; BitIndex < End; ++BitIndex)
		{
			SetBitNoCheck(BitIndex, bValue);
		}
	}

	/** Removes NumBitsToRemove bits at BaseIndex; later bits shift down (order kept). */
	void RemoveAt(int32 BaseIndex, int32 NumBitsToRemove = 1)
	{
		check(BaseIndex >= 0 && NumBitsToRemove >= 0 && BaseIndex + NumBitsToRemove <= NumBits);
		if (NumBitsToRemove == 0)
		{
			return;
		}
		for (int32 ReadIndex = BaseIndex + NumBitsToRemove, WriteIndex = BaseIndex; ReadIndex < NumBits;
			++ReadIndex, ++WriteIndex)
		{
			SetBitNoCheck(WriteIndex, GetBitNoCheck(ReadIndex));
		}
		NumBits -= NumBitsToRemove;
		ClearPartialSlackBits();
		ClearWordsFrom(GetNumWords());
	}

	/** Removes bits by moving the last ones into the hole (order not kept). */
	void RemoveAtSwap(int32 BaseIndex, int32 NumBitsToRemove = 1)
	{
		check(BaseIndex >= 0 && NumBitsToRemove >= 0 && BaseIndex + NumBitsToRemove <= NumBits);
		if (BaseIndex < NumBits - NumBitsToRemove)
		{
			// Copy bits from the end to the region we are removing.
			for (int32 Index = 0; Index < NumBitsToRemove; Index++)
			{
				const int32 FromIndex = NumBits - NumBitsToRemove + Index;
				// If the from index is within the part we are removing then we can skip it.
				if (FromIndex >= BaseIndex + NumBitsToRemove)
				{
					SetBitNoCheck(BaseIndex + Index, GetBitNoCheck(FromIndex));
				}
			}
		}
		NumBits -= NumBitsToRemove;
		ClearPartialSlackBits();
		ClearWordsFrom(GetNumWords());
	}

	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return InIndex >= 0 && InIndex < NumBits;
	}

	FORCEINLINE int32 Num() const
	{
		return NumBits;
	}

	FORCEINLINE int32 Max() const
	{
		return MaxBits;
	}

	FORCEINLINE FBitReference operator[](int32 Index)
	{
		checkf(Index >= 0 && Index < NumBits, "Bit index out of bounds: %d from %d", int(Index), int(NumBits));
		return FBitReference(GetData()[Index / NumBitsPerDWORD], 1u << (Index & (NumBitsPerDWORD - 1)));
	}

	FORCEINLINE const FConstBitReference operator[](int32 Index) const
	{
		checkf(Index >= 0 && Index < NumBits, "Bit index out of bounds: %d from %d", int(Index), int(NumBits));
		return FConstBitReference(GetData()[Index / NumBitsPerDWORD], 1u << (Index & (NumBitsPerDWORD - 1)));
	}

	FORCEINLINE FBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference)
	{
		checkSlow(RelativeReference.DWORDIndex < GetNumWords());
		return FBitReference(GetData()[RelativeReference.DWORDIndex], RelativeReference.Mask);
	}

	FORCEINLINE const FConstBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference) const
	{
		checkSlow(RelativeReference.DWORDIndex < GetNumWords());
		return FConstBitReference(GetData()[RelativeReference.DWORDIndex], RelativeReference.Mask);
	}

	/** Index of the first bit equal to bValue, or INDEX_NONE. */
	int32 Find(bool bValue) const
	{
		const WordType Test = bValue ? 0u : ~0u;
		const WordType* Data = GetData();
		const int32 NumWords = GetNumWords();
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			if (Data[WordIndex] != Test)
			{
				const WordType Bits = bValue ? Data[WordIndex] : ~Data[WordIndex];
				const int32 BitIndex = WordIndex * NumBitsPerDWORD + int32(FMath::CountTrailingZeros(Bits));
				return BitIndex < NumBits ? BitIndex : INDEX_NONE;
			}
		}
		return INDEX_NONE;
	}

	/** Index of the last bit equal to bValue, or INDEX_NONE. */
	int32 FindLast(bool bValue) const
	{
		for (int32 Index = NumBits - 1; Index >= 0; --Index)
		{
			if (GetBitNoCheck(Index) == bValue)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FORCEINLINE bool Contains(bool bValue) const
	{
		return Find(bValue) != INDEX_NONE;
	}

	/** Finds the first zero bit at or after ConservativeStartIndex, sets it and returns its index (INDEX_NONE if none).
	 */
	int32 FindAndSetFirstZeroBit(int32 ConservativeStartIndex = 0)
	{
		WordType* Data = GetData();
		const int32 NumWords = GetNumWords();
		for (int32 WordIndex = ConservativeStartIndex / NumBitsPerDWORD; WordIndex < NumWords; ++WordIndex)
		{
			if (Data[WordIndex] != ~0u)
			{
				const WordType Bits = ~Data[WordIndex];
				const int32 LowestBit = int32(FMath::CountTrailingZeros(Bits));
				const int32 BitIndex = WordIndex * NumBitsPerDWORD + LowestBit;
				if (BitIndex >= NumBits)
				{
					return INDEX_NONE;
				}
				Data[WordIndex] |= 1u << LowestBit;
				return BitIndex;
			}
		}
		return INDEX_NONE;
	}

	/** Finds the last zero bit, sets it and returns its index (INDEX_NONE if none). */
	int32 FindAndSetLastZeroBit()
	{
		const int32 Index = FindLast(false);
		if (Index != INDEX_NONE)
		{
			SetBitNoCheck(Index, true);
		}
		return Index;
	}

	/** Number of set bits in [FromIndex, ToIndex) (ToIndex = INDEX_NONE means Num()). */
	int32 CountSetBits(int32 FromIndex = 0, int32 ToIndex = INDEX_NONE) const
	{
		if (ToIndex == INDEX_NONE)
		{
			ToIndex = NumBits;
		}
		checkSlow(FromIndex >= 0 && ToIndex <= NumBits && FromIndex <= ToIndex);
		int32 Count = 0;
		if (FromIndex == 0 && ToIndex == NumBits)
		{
			const WordType* Data = GetData();
			for (int32 WordIndex = 0, NumWords = GetNumWords(); WordIndex < NumWords; ++WordIndex)
			{
				Count += FMath::CountBits(Data[WordIndex]);
			}
			return Count;
		}
		for (int32 Index = FromIndex; Index < ToIndex; ++Index)
		{
			Count += GetBitNoCheck(Index) ? 1 : 0;
		}
		return Count;
	}

	FORCEINLINE uint32* GetData()
	{
		return static_cast<uint32*>(AllocatorInstance.GetAllocation());
	}

	FORCEINLINE const uint32* GetData() const
	{
		return static_cast<const uint32*>(AllocatorInstance.GetAllocation());
	}

	SIZE_T GetAllocatedSize() const
	{
		return FMath::DivideAndRoundUp(MaxBits, int32(NumBitsPerDWORD)) * sizeof(WordType);
	}

	/** Index iterator over every bit (UE: TBitArray::FIterator / FConstIterator). */
	class FConstIterator
	{
	public:
		FORCEINLINE FConstIterator(const TBitArray& InArray, int32 StartIndex = 0)
			: Array(InArray)
			, Index(StartIndex)
		{
		}
		FORCEINLINE FConstIterator& operator++()
		{
			++Index;
			return *this;
		}
		FORCEINLINE explicit operator bool() const
		{
			return Index < Array.Num();
		}
		FORCEINLINE FConstBitReference GetValue() const
		{
			return Array[Index];
		}
		FORCEINLINE int32 GetIndex() const
		{
			return Index;
		}

	private:
		const TBitArray& Array;
		int32 Index;
	};

	class FIterator
	{
	public:
		FORCEINLINE FIterator(TBitArray& InArray, int32 StartIndex = 0)
			: Array(InArray)
			, Index(StartIndex)
		{
		}
		FORCEINLINE FIterator& operator++()
		{
			++Index;
			return *this;
		}
		FORCEINLINE explicit operator bool() const
		{
			return Index < Array.Num();
		}
		FORCEINLINE FBitReference GetValue() const
		{
			return Array[Index];
		}
		FORCEINLINE int32 GetIndex() const
		{
			return Index;
		}

	private:
		TBitArray& Array;
		int32 Index;
	};

private:
	FORCEINLINE int32 GetNumWords() const
	{
		return FMath::DivideAndRoundUp(NumBits, int32(NumBitsPerDWORD));
	}

	FORCEINLINE bool GetBitNoCheck(int32 Index) const
	{
		return (GetData()[Index >> NumBitsPerDWORDLogTwo] & (1u << (Index & (NumBitsPerDWORD - 1)))) != 0;
	}

	FORCEINLINE void SetBitNoCheck(int32 Index, bool bValue)
	{
		WordType& Word = GetData()[Index >> NumBitsPerDWORDLogTwo];
		const WordType Mask = 1u << (Index & (NumBitsPerDWORD - 1));
		Word = bValue ? (Word | Mask) : (Word & ~Mask);
	}

	/** Zeroes the bits of the last word past NumBits. */
	FORCEINLINE void ClearPartialSlackBits()
	{
		const int32 UsedBits = NumBits % NumBitsPerDWORD;
		if (UsedBits != 0)
		{
			GetData()[NumBits / NumBitsPerDWORD] &= (1u << UsedBits) - 1u;
		}
	}

	/** Zeroes whole words from FirstWord up to the allocation's end. */
	FORCEINLINE void ClearWordsFrom(int32 FirstWord)
	{
		const int32 MaxWords = MaxBits / NumBitsPerDWORD;
		if (FirstWord < MaxWords)
		{
			FMemory::Memzero(GetData() + FirstWord, (MaxWords - FirstWord) * sizeof(WordType));
		}
	}

	FORCENOINLINE void Realloc(int32 PreviousNumWords, int32 MaxDWORDs)
	{
		AllocatorInstance.ResizeAllocation(PreviousNumWords, MaxDWORDs, sizeof(WordType));
		MaxBits = MaxDWORDs * NumBitsPerDWORD;
		// Keep the invariant for the words that were not copied over.
		ClearWordsFrom(PreviousNumWords);
	}

	static FORCEINLINE void MoveOrCopy(TBitArray& ToArray, TBitArray& FromArray)
	{
		if constexpr (TAllocatorTraits<Allocator>::SupportsMove)
		{
			ToArray.AllocatorInstance.MoveToEmpty(FromArray.AllocatorInstance);
			ToArray.NumBits = FromArray.NumBits;
			ToArray.MaxBits = FromArray.MaxBits;
			FromArray.NumBits = 0;
			FromArray.MaxBits = FromArray.AllocatorInstance.GetInitialCapacity() * NumBitsPerDWORD;
			FromArray.ClearWordsFrom(0);
		}
		else
		{
			ToArray = FromArray;
		}
	}

	AllocatorType AllocatorInstance;
	int32 NumBits;
	int32 MaxBits;
};

/** Iterates the indices of the set bits (UE: TConstSetBitIterator). */
template <typename Allocator /* = FDefaultBitArrayAllocator */>
class TConstSetBitIterator
{
public:
	TConstSetBitIterator(const TBitArray<Allocator>& InArray, int32 StartIndex = 0)
		: Array(InArray)
		, CurrentBitIndex(StartIndex)
	{
		check(StartIndex >= 0 && StartIndex <= Array.Num());
		FindNextSetBit();
	}

	FORCEINLINE TConstSetBitIterator& operator++()
	{
		++CurrentBitIndex;
		FindNextSetBit();
		return *this;
	}

	FORCEINLINE bool operator==(const TConstSetBitIterator& Rhs) const
	{
		return CurrentBitIndex == Rhs.CurrentBitIndex && &Array == &Rhs.Array;
	}
	FORCEINLINE bool operator!=(const TConstSetBitIterator& Rhs) const
	{
		return !(*this == Rhs);
	}

	FORCEINLINE explicit operator bool() const
	{
		return CurrentBitIndex < Array.Num();
	}

	FORCEINLINE int32 GetIndex() const
	{
		return CurrentBitIndex;
	}

private:
	void FindNextSetBit()
	{
		const int32 NumBits = Array.Num();
		const uint32* Data = Array.GetData();
		while (CurrentBitIndex < NumBits)
		{
			const int32 WordIndex = CurrentBitIndex >> NumBitsPerDWORDLogTwo;
			// Bits at or after the current index in this word.
			const uint32 Word = Data[WordIndex] & (~0u << (CurrentBitIndex & (NumBitsPerDWORD - 1)));
			if (Word)
			{
				CurrentBitIndex = WordIndex * NumBitsPerDWORD + int32(FMath::CountTrailingZeros(Word));
				if (CurrentBitIndex > NumBits)
				{
					CurrentBitIndex = NumBits;
				}
				return;
			}
			CurrentBitIndex = (WordIndex + 1) * NumBitsPerDWORD;
		}
		CurrentBitIndex = NumBits;
	}

	const TBitArray<Allocator>& Array;
	int32 CurrentBitIndex;
};

/**
 * Untyped view of a TBitArray for the reflection system (UE: TScriptBitArray). Same layout as
 * TBitArray<Allocator>, and the same invariant: the words past NumBits inside the allocation are zero.
 */
template <typename Allocator /* = FDefaultBitArrayAllocator */>
class TScriptBitArray
{
public:
	TScriptBitArray()
		: NumBits(0)
		, MaxBits(0)
	{
	}

	TScriptBitArray(const TScriptBitArray&) = delete;
	TScriptBitArray& operator=(const TScriptBitArray&) = delete;

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < NumBits;
	}

	FORCEINLINE int32 Num() const
	{
		return NumBits;
	}

	FORCEINLINE FBitReference operator[](int32 Index)
	{
		check(IsValidIndex(Index));
		return FBitReference(GetData()[Index / NumBitsPerDWORD], 1u << (Index & (NumBitsPerDWORD - 1)));
	}

	FORCEINLINE const FConstBitReference operator[](int32 Index) const
	{
		check(IsValidIndex(Index));
		return FConstBitReference(GetData()[Index / NumBitsPerDWORD], 1u << (Index & (NumBitsPerDWORD - 1)));
	}

	/** Takes Other's bits (TBitArray's move). */
	void MoveAssign(TScriptBitArray& Other)
	{
		checkSlow(this != &Other);
		Empty(0);
		AllocatorInstance.MoveToEmpty(Other.AllocatorInstance);
		NumBits = Other.NumBits;
		MaxBits = Other.MaxBits;
		Other.NumBits = 0;
		Other.MaxBits = Other.AllocatorInstance.GetInitialCapacity() * NumBitsPerDWORD;
		Other.ClearWordsFrom(0);
	}

	/** Removes every bit; the allocation shrinks / grows to ExpectedNumBits (TBitArray::Empty). */
	void Empty(int32 ExpectedNumBits = 0)
	{
		ExpectedNumBits = FMath::DivideAndRoundUp(ExpectedNumBits, int32(NumBitsPerDWORD)) * NumBitsPerDWORD;
		const int32 InitialMaxBits = AllocatorInstance.GetInitialCapacity() * NumBitsPerDWORD;
		NumBits = 0;
		if (ExpectedNumBits > MaxBits || MaxBits > InitialMaxBits)
		{
			Realloc(0, FMath::Max(ExpectedNumBits, InitialMaxBits) / NumBitsPerDWORD);
		}
		else
		{
			ClearWordsFrom(0);
		}
	}

	/** Appends a bit; returns its index (TBitArray::Add). */
	int32 Add(const bool bValue)
	{
		const int32 Index = NumBits;
		const int32 OldNumWords = GetNumWords();
		++NumBits;
		if (NumBits > MaxBits)
		{
			const int32 MaxDWORDs = AllocatorInstance.CalculateSlackGrow(
				FMath::DivideAndRoundUp(NumBits, int32(NumBitsPerDWORD)), MaxBits / NumBitsPerDWORD, sizeof(uint32));
			Realloc(OldNumWords, MaxDWORDs);
		}
		const int32 NewNumWords = GetNumWords();
		if (NewNumWords > OldNumWords)
		{
			FMemory::Memzero(GetData() + OldNumWords, (NewNumWords - OldNumWords) * sizeof(uint32));
		}
		(*this)[Index] = bValue;
		return Index;
	}

	/** The script bit array must be a drop-in view of TBitArray (UE: TScriptBitArray::CheckConstraints). */
	static void CheckConstraints()
	{
		typedef TScriptBitArray ScriptType;
		typedef TBitArray<Allocator> RealType;
		static_assert(sizeof(ScriptType) == sizeof(RealType), "TScriptBitArray's size doesn't match TBitArray");
		static_assert(alignof(ScriptType) == alignof(RealType), "TScriptBitArray's alignment doesn't match TBitArray");
		static_assert(offsetof(ScriptType, AllocatorInstance) == offsetof(RealType, AllocatorInstance),
			"TScriptBitArray's allocator offset doesn't match TBitArray");
		static_assert(offsetof(ScriptType, NumBits) == offsetof(RealType, NumBits),
			"TScriptBitArray's NumBits offset doesn't match TBitArray");
		static_assert(offsetof(ScriptType, MaxBits) == offsetof(RealType, MaxBits),
			"TScriptBitArray's MaxBits offset doesn't match TBitArray");
	}

private:
	typedef typename Allocator::template ForElementType<uint32> AllocatorType;

	FORCEINLINE uint32* GetData()
	{
		return static_cast<uint32*>(AllocatorInstance.GetAllocation());
	}
	FORCEINLINE const uint32* GetData() const
	{
		return static_cast<const uint32*>(AllocatorInstance.GetAllocation());
	}

	FORCEINLINE int32 GetNumWords() const
	{
		return FMath::DivideAndRoundUp(NumBits, int32(NumBitsPerDWORD));
	}

	FORCEINLINE void ClearWordsFrom(int32 FirstWord)
	{
		const int32 MaxWords = MaxBits / NumBitsPerDWORD;
		if (FirstWord < MaxWords)
		{
			FMemory::Memzero(GetData() + FirstWord, (MaxWords - FirstWord) * sizeof(uint32));
		}
	}

	FORCENOINLINE void Realloc(int32 PreviousNumWords, int32 MaxDWORDs)
	{
		AllocatorInstance.ResizeAllocation(PreviousNumWords, MaxDWORDs, sizeof(uint32));
		MaxBits = MaxDWORDs * NumBitsPerDWORD;
		ClearWordsFrom(PreviousNumWords);
	}

	AllocatorType AllocatorInstance;
	int32 NumBits;
	int32 MaxBits;
};
