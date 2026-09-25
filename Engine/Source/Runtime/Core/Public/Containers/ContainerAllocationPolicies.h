#pragma once

#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Templates/MemoryOps.h"
#include "Templates/TypeCompatibleBytes.h"

#include <limits>
#include <type_traits>

// Container allocators (UE: Containers/ContainerAllocationPolicies.h). An allocator owns a block of raw memory; the
// container constructs / destroys the elements inside it.

template <int IndexSize>
struct TBitsToSizeType;
template <>
struct TBitsToSizeType<8>
{
	using Type = int8;
};
template <>
struct TBitsToSizeType<16>
{
	using Type = int16;
};
template <>
struct TBitsToSizeType<32>
{
	using Type = int32;
};
template <>
struct TBitsToSizeType<64>
{
	using Type = int64;
};

// Growth / shrink policy shared by the heap allocators (UE: DefaultCalculateSlack*).

template <typename SizeType>
FORCEINLINE SizeType DefaultCalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements,
	SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	SizeType Retval;
	checkSlow(NumElements < NumAllocatedElements);

	// If the container has too much slack, shrink it to exactly fit the number of elements.
	const SizeType CurrentSlackElements = NumAllocatedElements - NumElements;
	const SIZE_T CurrentSlackBytes = SIZE_T(CurrentSlackElements) * BytesPerElement;
	const bool bTooManySlackBytes = CurrentSlackBytes >= 16384;
	const bool bTooManySlackElements = 3 * NumElements < 2 * NumAllocatedElements;
	if ((bTooManySlackBytes || bTooManySlackElements) && (CurrentSlackElements > 64 || !NumElements))
	{
		Retval = NumElements;
		if (Retval > 0 && bAllowQuantize)
		{
			Retval =
				static_cast<SizeType>(FMemory::QuantizeSize(Retval * BytesPerElement, Alignment) / BytesPerElement);
		}
	}
	else
	{
		Retval = NumAllocatedElements;
	}
	return Retval;
}

template <typename SizeType>
FORCEINLINE SizeType DefaultCalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements,
	SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	constexpr SIZE_T FirstGrow = 4;
	constexpr SIZE_T ConstantGrow = 16;

	SizeType Retval;
	checkSlow(NumElements > NumAllocatedElements && NumElements > 0);

	SIZE_T Grow = FirstGrow;
	if (NumAllocatedElements || SIZE_T(NumElements) > Grow)
	{
		// Allocate slack for the array proportional to its size.
		Grow = SIZE_T(NumElements) + 3 * SIZE_T(NumElements) / 8 + ConstantGrow;
	}
	if (bAllowQuantize)
	{
		Retval = static_cast<SizeType>(FMemory::QuantizeSize(Grow * BytesPerElement, Alignment) / BytesPerElement);
	}
	else
	{
		Retval = static_cast<SizeType>(Grow);
	}
	// NumElements and MaxElements are stored in 32-bit signed integers, so clamp to the max value.
	if (NumElements > Retval)
	{
		Retval = std::numeric_limits<SizeType>::max();
	}
	return Retval;
}

template <typename SizeType>
FORCEINLINE SizeType DefaultCalculateSlackReserve(
	SizeType NumElements, SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	SizeType Retval = NumElements;
	checkSlow(NumElements > 0);
	if (bAllowQuantize)
	{
		Retval =
			static_cast<SizeType>(FMemory::QuantizeSize(SIZE_T(Retval) * BytesPerElement, Alignment) / BytesPerElement);
		if (NumElements > Retval)
		{
			Retval = std::numeric_limits<SizeType>::max();
		}
	}
	return Retval;
}

/** Heap alignment for an element type: the allocator default unless the type asks for more. */
template <typename ElementType>
constexpr uint32 GetContainerAlignment()
{
	return alignof(ElementType) > 16 ? uint32(alignof(ElementType)) : uint32(DEFAULT_ALIGNMENT);
}

/** Capabilities a container checks on its allocator (UE: TAllocatorTraits). */
template <typename AllocatorType>
struct TAllocatorTraitsBase
{
	enum
	{
		SupportsMove = false
	};
	enum
	{
		IsZeroConstruct = false
	};
};

template <typename AllocatorType>
struct TAllocatorTraits : TAllocatorTraitsBase<AllocatorType>
{
};

/** Heap allocator of a single block (UE: TSizedHeapAllocator). */
template <int IndexSize>
class TSizedHeapAllocator
{
public:
	using SizeType = typename TBitsToSizeType<IndexSize>::Type;

	enum
	{
		NeedsElementType = false
	};
	enum
	{
		RequireRangeCheck = true
	};

	class ForAnyElementType
	{
	public:
		ForAnyElementType() = default;
		ForAnyElementType(const ForAnyElementType&) = delete;
		ForAnyElementType& operator=(const ForAnyElementType&) = delete;

		FORCEINLINE ~ForAnyElementType()
		{
			if (Data)
			{
				FMemory::Free(Data);
			}
		}

		/** Takes Other's allocation; this allocator must hold no elements. */
		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			checkSlow(this != &Other);
			if (Data)
			{
				FMemory::Free(Data);
			}
			Data = Other.Data;
			Other.Data = nullptr;
		}

		FORCEINLINE void* GetAllocation() const
		{
			return Data;
		}

		FORCEINLINE void ResizeAllocation(SizeType /*PreviousNumElements*/, SizeType NumElements,
			SIZE_T NumBytesPerElement, uint32 AlignmentOfElement = DEFAULT_ALIGNMENT)
		{
			// Avoid calling FMemory::Realloc(nullptr, 0) (it would allocate).
			if (Data || NumElements)
			{
				Data = FMemory::Realloc(Data, SIZE_T(NumElements) * NumBytesPerElement, AlignmentOfElement);
			}
		}

		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackShrink(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackGrow(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}

		SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return SIZE_T(NumAllocatedElements) * NumBytesPerElement;
		}

		bool HasAllocation() const
		{
			return !!Data;
		}

		SizeType GetInitialCapacity() const
		{
			return 0;
		}

	private:
		void* Data = nullptr;
	};

	template <typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:
		ForElementType() = default;

		FORCEINLINE ElementType* GetAllocation() const
		{
			return static_cast<ElementType*>(ForAnyElementType::GetAllocation());
		}
	};
};

template <int IndexSize>
struct TAllocatorTraits<TSizedHeapAllocator<IndexSize>> : TAllocatorTraitsBase<TSizedHeapAllocator<IndexSize>>
{
	enum
	{
		SupportsMove = true
	};
	enum
	{
		IsZeroConstruct = true
	};
};

using FHeapAllocator = TSizedHeapAllocator<32>;

/** The allocator TArray uses by default (UE: TSizedDefaultAllocator / FDefaultAllocator). */
template <int IndexSize>
class TSizedDefaultAllocator : public TSizedHeapAllocator<IndexSize>
{
public:
	typedef TSizedHeapAllocator<IndexSize> Typedef;
};

template <int IndexSize>
struct TAllocatorTraits<TSizedDefaultAllocator<IndexSize>> : TAllocatorTraits<TSizedHeapAllocator<IndexSize>>
{
};

/**
 * Keeps up to NumInlineElements inside the container and moves to the secondary allocator beyond that
 * (UE: TInlineAllocator).
 */
template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultAllocator>
class TInlineAllocator
{
public:
	using SizeType = int32;

	enum
	{
		NeedsElementType = true
	};
	enum
	{
		RequireRangeCheck = true
	};

	template <typename ElementType>
	class ForElementType
	{
	public:
		ForElementType() = default;
		ForElementType(const ForElementType&) = delete;
		ForElementType& operator=(const ForElementType&) = delete;

		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			checkSlow(this != &Other);
			if (!Other.SecondaryData.GetAllocation())
			{
				// Relocate the other container's inline elements into this one.
				RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
			}
			// Move the secondary allocation (and free this container's own, if any).
			SecondaryData.MoveToEmpty(Other.SecondaryData);
		}

		FORCEINLINE ElementType* GetAllocation() const
		{
			ElementType* Secondary = SecondaryData.GetAllocation();
			return Secondary ? Secondary : GetInlineElements();
		}

		void ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement,
			uint32 AlignmentOfElement = DEFAULT_ALIGNMENT)
		{
			if (NumElements <= SizeType(NumInlineElements))
			{
				// Back to the inline storage: relocate the elements out of the secondary block and free it.
				if (SecondaryData.GetAllocation())
				{
					RelocateConstructItems<ElementType>(
						(void*)InlineData, (ElementType*)SecondaryData.GetAllocation(), PreviousNumElements);
					SecondaryData.ResizeAllocation(0, 0, NumBytesPerElement, AlignmentOfElement);
				}
			}
			else if (!SecondaryData.GetAllocation())
			{
				// Leaving the inline storage.
				SecondaryData.ResizeAllocation(0, NumElements, NumBytesPerElement, AlignmentOfElement);
				RelocateConstructItems<ElementType>(
					(void*)SecondaryData.GetAllocation(), GetInlineElements(), PreviousNumElements);
			}
			else
			{
				SecondaryData.ResizeAllocation(
					PreviousNumElements, NumElements, NumBytesPerElement, AlignmentOfElement);
			}
		}

		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return NumElements <= SizeType(NumInlineElements)
				? SizeType(NumInlineElements)
				: SecondaryData.CalculateSlackReserve(NumElements, NumBytesPerElement);
		}
		FORCEINLINE SizeType CalculateSlackShrink(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumElements <= SizeType(NumInlineElements)
				? SizeType(NumInlineElements)
				: SecondaryData.CalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement);
		}
		FORCEINLINE SizeType CalculateSlackGrow(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumElements <= SizeType(NumInlineElements)
				? SizeType(NumInlineElements)
				: SecondaryData.CalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement);
		}

		SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			if (NumAllocatedElements > SizeType(NumInlineElements))
			{
				return SecondaryData.GetAllocatedSize(NumAllocatedElements, NumBytesPerElement);
			}
			return 0;
		}

		bool HasAllocation() const
		{
			return SecondaryData.HasAllocation();
		}

		SizeType GetInitialCapacity() const
		{
			return NumInlineElements;
		}

	private:
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}

		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];
		typename SecondaryAllocator::template ForElementType<ElementType> SecondaryData;
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements, typename SecondaryAllocator>
struct TAllocatorTraits<TInlineAllocator<NumInlineElements, SecondaryAllocator>>
	: TAllocatorTraitsBase<TInlineAllocator<NumInlineElements, SecondaryAllocator>>
{
	enum
	{
		SupportsMove = TAllocatorTraits<SecondaryAllocator>::SupportsMove
	};
};

/** Inline storage only; exceeding NumInlineElements is a fatal error (UE: TFixedAllocator). */
template <uint32 NumInlineElements>
class TFixedAllocator
{
public:
	using SizeType = int32;

	enum
	{
		NeedsElementType = true
	};
	enum
	{
		RequireRangeCheck = true
	};

	template <typename ElementType>
	class ForElementType
	{
	public:
		ForElementType() = default;
		ForElementType(const ForElementType&) = delete;
		ForElementType& operator=(const ForElementType&) = delete;

		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			checkSlow(this != &Other);
			RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
		}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return GetInlineElements();
		}

		void ResizeAllocation(SizeType /*PreviousNumElements*/, SizeType NumElements, SIZE_T /*NumBytesPerElement*/,
			uint32 /*AlignmentOfElement*/ = DEFAULT_ALIGNMENT)
		{
			checkf(NumElements <= SizeType(NumInlineElements), "TFixedAllocator overflow: %d > %u", int(NumElements),
				unsigned(NumInlineElements));
		}

		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T /*NumBytesPerElement*/) const
		{
			checkf(NumElements <= SizeType(NumInlineElements), "TFixedAllocator overflow: %d > %u", int(NumElements),
				unsigned(NumInlineElements));
			return NumInlineElements;
		}
		FORCEINLINE SizeType CalculateSlackShrink(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T /*NumBytesPerElement*/) const
		{
			checkSlow(NumAllocatedElements <= SizeType(NumInlineElements));
			(void)NumElements;
			return NumInlineElements;
		}
		FORCEINLINE SizeType CalculateSlackGrow(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T /*NumBytesPerElement*/) const
		{
			checkf(NumElements <= SizeType(NumInlineElements), "TFixedAllocator overflow: %d > %u", int(NumElements),
				unsigned(NumInlineElements));
			(void)NumAllocatedElements;
			return NumInlineElements;
		}

		SIZE_T GetAllocatedSize(SizeType /*NumAllocatedElements*/, SIZE_T /*NumBytesPerElement*/) const
		{
			return 0;
		}

		bool HasAllocation() const
		{
			return false;
		}

		SizeType GetInitialCapacity() const
		{
			return NumInlineElements;
		}

	private:
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}

		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements>
struct TAllocatorTraits<TFixedAllocator<NumInlineElements>> : TAllocatorTraitsBase<TFixedAllocator<NumInlineElements>>
{
	enum
	{
		SupportsMove = true
	};
};

// Allocator bundles of the sparse array and the set.

/** Element + presence-bit allocators of a TSparseArray (UE: TSparseArrayAllocator). */
template <typename InElementAllocator = FDefaultAllocator, typename InBitArrayAllocator = FDefaultBitArrayAllocator>
class TSparseArrayAllocator
{
public:
	typedef InElementAllocator ElementAllocator;
	typedef InBitArrayAllocator BitArrayAllocator;
};

/** Sparse array + hash allocators of a TSet (UE: TSetAllocator). */
template <typename InSparseArrayAllocator = TSparseArrayAllocator<>,
	typename InHashAllocator = TInlineAllocator<1, FDefaultAllocator>, uint32 AverageNumberOfElementsPerHashBucket = 2,
	uint32 BaseNumberOfHashBuckets = 8, uint32 MinNumberOfHashedElements = 4>
class TSetAllocator
{
public:
	/** Hash bucket count for a number of elements: a power of two. */
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		if (NumHashedElements >= MinNumberOfHashedElements)
		{
			return FMath::RoundUpToPowerOfTwo(
				NumHashedElements / AverageNumberOfElementsPerHashBucket + BaseNumberOfHashBuckets);
		}
		return 1;
	}

	typedef InSparseArrayAllocator SparseArrayAllocator;
	typedef InHashAllocator HashAllocator;
};

class FDefaultBitArrayAllocator : public TInlineAllocator<4>
{
public:
	typedef TInlineAllocator<4> Typedef;
};
template <>
struct TAllocatorTraits<FDefaultBitArrayAllocator> : TAllocatorTraits<TInlineAllocator<4>>
{
};

class FDefaultSparseArrayAllocator : public TSparseArrayAllocator<>
{
public:
	typedef TSparseArrayAllocator<> Typedef;
};

class FDefaultSetAllocator : public TSetAllocator<>
{
public:
	typedef TSetAllocator<> Typedef;
};

/** Inline allocators for small sets (UE: TInlineSparseArrayAllocator / TInlineSetAllocator). */
template <uint32 NumInlineElements,
	typename SecondaryAllocator = TSparseArrayAllocator<FDefaultAllocator, FDefaultAllocator>>
class TInlineSparseArrayAllocator
{
public:
	typedef TInlineAllocator<NumInlineElements, typename SecondaryAllocator::ElementAllocator> ElementAllocator;
	typedef TInlineAllocator<(NumInlineElements + 31) / 32, typename SecondaryAllocator::BitArrayAllocator>
		BitArrayAllocator;
};

template <uint32 NumInlineElements,
	typename SecondaryAllocator =
		TSetAllocator<TSparseArrayAllocator<FDefaultAllocator, FDefaultAllocator>, FDefaultAllocator>>
class TInlineSetAllocator
{
private:
	enum
	{
		NumInlineHashBuckets = (NumInlineElements + 2 - 1) / 2
	};

public:
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		const uint32 NumDesiredHashBuckets = FMath::RoundUpToPowerOfTwo(NumHashedElements / 2);
		if (NumDesiredHashBuckets < NumInlineHashBuckets)
		{
			return FMath::RoundUpToPowerOfTwo(uint32(NumInlineHashBuckets));
		}
		if (NumHashedElements < 4)
		{
			return 1;
		}
		return NumDesiredHashBuckets;
	}

	typedef TInlineSparseArrayAllocator<NumInlineElements, typename SecondaryAllocator::SparseArrayAllocator>
		SparseArrayAllocator;
	typedef TInlineAllocator<NumInlineHashBuckets, typename SecondaryAllocator::HashAllocator> HashAllocator;
};
