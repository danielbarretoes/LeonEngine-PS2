#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"

#include <cstddef>

/**
 * Untyped TArray for code that only knows the element size, such as the reflection system (UE: TScriptArray). Its
 * layout is the one of TArray<T, AllocatorType> for every T, so an FScriptArray can be read or modified through a
 * TArray and back. It never constructs or destroys elements: callers do that (FScriptArrayHelper).
 */
template <typename AllocatorType>
class TScriptArray
{
public:
	typedef typename AllocatorType::ForAnyElementType ElementAllocatorType;

	FORCEINLINE void* GetData()
	{
		return AllocatorInstance.GetAllocation();
	}
	FORCEINLINE const void* GetData() const
	{
		return AllocatorInstance.GetAllocation();
	}

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < ArrayNum;
	}

	FORCEINLINE int32 Num() const
	{
		checkSlow(ArrayNum >= 0 && ArrayMax >= ArrayNum);
		return ArrayNum;
	}

	FORCEINLINE int32 Max() const
	{
		return ArrayMax;
	}

	FORCEINLINE int32 GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}

	/** Inserts Count zeroed elements at Index. */
	void InsertZeroed(int32 Index, int32 Count, int32 NumBytesPerElement)
	{
		Insert(Index, Count, NumBytesPerElement);
		FMemory::Memzero(
			(uint8*)AllocatorInstance.GetAllocation() + Index * NumBytesPerElement, Count * NumBytesPerElement);
	}

	/** Inserts Count uninitialized elements at Index. */
	void Insert(int32 Index, int32 Count, int32 NumBytesPerElement)
	{
		check(Count >= 0);
		check(ArrayNum >= 0 && ArrayMax >= ArrayNum);
		check(Index >= 0 && Index <= ArrayNum);

		const int32 OldNum = ArrayNum;
		if ((ArrayNum += Count) > ArrayMax)
		{
			ResizeGrow(OldNum, NumBytesPerElement);
		}
		FMemory::Memmove((uint8*)AllocatorInstance.GetAllocation() + (Index + Count) * NumBytesPerElement,
			(uint8*)AllocatorInstance.GetAllocation() + Index * NumBytesPerElement,
			(OldNum - Index) * NumBytesPerElement);
	}

	/** Adds Count uninitialized elements; returns the index of the first. */
	int32 Add(int32 Count, int32 NumBytesPerElement)
	{
		check(Count >= 0);
		checkSlow(ArrayNum >= 0 && ArrayMax >= ArrayNum);

		const int32 OldNum = ArrayNum;
		if ((ArrayNum += Count) > ArrayMax)
		{
			ResizeGrow(OldNum, NumBytesPerElement);
		}
		return OldNum;
	}

	/** Adds Count zeroed elements; returns the index of the first. */
	int32 AddZeroed(int32 Count, int32 NumBytesPerElement)
	{
		const int32 Index = Add(Count, NumBytesPerElement);
		FMemory::Memzero(
			(uint8*)AllocatorInstance.GetAllocation() + Index * NumBytesPerElement, Count * NumBytesPerElement);
		return Index;
	}

	/** Releases the slack. */
	void Shrink(int32 NumBytesPerElement)
	{
		checkSlow(ArrayNum >= 0 && ArrayMax >= ArrayNum);
		if (ArrayNum != ArrayMax)
		{
			ResizeTo(ArrayNum, NumBytesPerElement);
		}
	}

	/** Removes every element (already destroyed by the caller), keeping room for Slack elements. */
	void Empty(int32 Slack, int32 NumBytesPerElement)
	{
		checkSlow(Slack >= 0);
		ArrayNum = 0;
		if (Slack != ArrayMax)
		{
			ResizeTo(Slack, NumBytesPerElement);
		}
	}

	/** Swaps the bytes of two elements. */
	void SwapMemory(int32 A, int32 B, int32 NumBytesPerElement)
	{
		uint8* Data = (uint8*)AllocatorInstance.GetAllocation();
		uint8* First = Data + A * NumBytesPerElement;
		uint8* Second = Data + B * NumBytesPerElement;
		for (int32 Index = 0; Index < NumBytesPerElement; ++Index)
		{
			const uint8 Temp = First[Index];
			First[Index] = Second[Index];
			Second[Index] = Temp;
		}
	}

	/** Removes Count elements (already destroyed by the caller) at Index, keeping the order of the others. */
	void Remove(int32 Index, int32 Count, int32 NumBytesPerElement)
	{
		if (Count)
		{
			check(Count >= 0 && Index >= 0 && Index + Count <= ArrayNum);
			const int32 NumToMove = ArrayNum - Index - Count;
			if (NumToMove)
			{
				FMemory::Memmove((uint8*)AllocatorInstance.GetAllocation() + Index * NumBytesPerElement,
					(uint8*)AllocatorInstance.GetAllocation() + (Index + Count) * NumBytesPerElement,
					NumToMove * NumBytesPerElement);
			}
			ArrayNum -= Count;
			ResizeShrink(NumBytesPerElement);
		}
	}

	/** Takes Other's allocation; this array must hold no constructed elements. */
	void MoveAssign(TScriptArray& Other, int32 NumBytesPerElement)
	{
		checkSlow(this != &Other);
		Empty(0, NumBytesPerElement);
		AllocatorInstance.MoveToEmpty(Other.AllocatorInstance);
		ArrayNum = Other.ArrayNum;
		ArrayMax = Other.ArrayMax;
		Other.ArrayNum = 0;
		Other.ArrayMax = 0;
	}

	TScriptArray()
		: ArrayNum(0)
		, ArrayMax(0)
	{
	}

	TScriptArray(const TScriptArray&) = delete;
	TScriptArray& operator=(const TScriptArray&) = delete;

protected:
	ElementAllocatorType AllocatorInstance;
	int32 ArrayNum;
	int32 ArrayMax;

	FORCENOINLINE void ResizeGrow(int32 OldNum, int32 NumBytesPerElement)
	{
		ArrayMax = AllocatorInstance.CalculateSlackGrow(ArrayNum, ArrayMax, NumBytesPerElement);
		AllocatorInstance.ResizeAllocation(OldNum, ArrayMax, NumBytesPerElement);
	}

	FORCENOINLINE void ResizeShrink(int32 NumBytesPerElement)
	{
		const int32 NewArrayMax = AllocatorInstance.CalculateSlackShrink(ArrayNum, ArrayMax, NumBytesPerElement);
		if (NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			AllocatorInstance.ResizeAllocation(ArrayNum, ArrayMax, NumBytesPerElement);
		}
	}

	FORCENOINLINE void ResizeTo(int32 NewMax, int32 NumBytesPerElement)
	{
		if (NewMax)
		{
			NewMax = AllocatorInstance.CalculateSlackReserve(NewMax, NumBytesPerElement);
		}
		if (NewMax != ArrayMax)
		{
			ArrayMax = NewMax;
			AllocatorInstance.ResizeAllocation(ArrayNum, ArrayMax, NumBytesPerElement);
		}
	}

public:
	/** The script array must be a drop-in view of TArray (UE: TScriptArray::CheckConstraints). */
	static void CheckConstraints()
	{
		typedef TScriptArray ScriptType;
		typedef TArray<int32, AllocatorType> RealType;
		static_assert(sizeof(ScriptType) == sizeof(RealType), "TScriptArray's size doesn't match TArray");
		static_assert(alignof(ScriptType) == alignof(RealType), "TScriptArray's alignment doesn't match TArray");
		static_assert(sizeof(ScriptType::ArrayNum) == sizeof(typename RealType::SizeType),
			"TScriptArray's ArrayNum size doesn't match TArray");
		static_assert(offsetof(ScriptType, AllocatorInstance) == offsetof(RealType, AllocatorInstance),
			"TScriptArray's allocator offset doesn't match TArray");
		static_assert(offsetof(ScriptType, ArrayNum) == offsetof(RealType, ArrayNum),
			"TScriptArray's ArrayNum offset doesn't match TArray");
		static_assert(offsetof(ScriptType, ArrayMax) == offsetof(RealType, ArrayMax),
			"TScriptArray's ArrayMax offset doesn't match TArray");
	}
};

/** TArray of any element type with the default allocator (UE: FScriptArray). */
class FScriptArray : public TScriptArray<FHeapAllocator>
{
public:
	FScriptArray() = default;

	FScriptArray(const FScriptArray&) = delete;
	FScriptArray& operator=(const FScriptArray&) = delete;
};

static_assert(sizeof(FScriptArray) == sizeof(TArray<int32>), "FScriptArray's size doesn't match TArray");
static_assert(sizeof(FScriptArray) == sizeof(TArray<uint8>), "FScriptArray's size doesn't match TArray");
