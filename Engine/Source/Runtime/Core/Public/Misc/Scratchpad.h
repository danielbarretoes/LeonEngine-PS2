#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Misc/MemStack.h"

/**
 * The EE's scratchpad (Leon, Docs/PLANS/ps2-shipping.md N15): 16 KB of on-chip RAM at 0x70000000 that the EE reads
 * and writes in one cycle, outside the data cache, as a stack of the frame's hottest temporaries: the scene renderer's
 * culling lists and the emitter's per-batch vertices. Every platform has one of the same size (the PC a static
 * buffer, and the PS2 too with -nospr, for a measurement), so what fits on the PS2 fits everywhere.
 *
 * - PushBytes bumps a pointer; an FScratchpadMark gives back what was pushed after it, as FMemMark does.
 * - What does not fit goes to the frame's stack instead (FMemStack, inside the caller's FMemMark) and is counted
 *   (GetNumOverflows), so a full scratchpad slows a frame down and never fails it.
 * - Nothing a DMA channel reads may live in it: the DMA controller reaches the scratchpad only through its own
 *   channels (fromSPR / toSPR), not by an address in a chain.
 * - One thread: the game thread's.
 */
class CORE_API FScratchpad
{
public:
	/** The EE's scratchpad RAM. */
	static constexpr SIZE_T Size = 16 * 1024;

	/** The game thread's scratchpad. */
	static FScratchpad& Get();

	/**
	 * AllocSize bytes aligned to Alignment (a power of two) until the enclosing mark pops: from the scratchpad, or from
	 * the frame's stack when it is full.
	 */
	uint8* PushBytes(SIZE_T AllocSize, SIZE_T Alignment);

	/**
	 * Grows the block at the top of the scratchpad from OldSize to NewSize bytes in place; false when Block is not the
	 * top block or the scratchpad has no room (the caller then pushes a new block).
	 */
	bool GrowTop(const uint8* Block, SIZE_T OldSize, SIZE_T NewSize);

	/** Whether Pointer is in the scratchpad's memory. */
	[[nodiscard]] bool Contains(const void* Pointer) const
	{
		return Pointer >= Base && Pointer < Base + Size;
	}

	/** Whether the scratchpad is the EE's on-chip RAM (false on the PC, and on the PS2 with -nospr). */
	[[nodiscard]] bool IsOnChip() const
	{
		return bOnChip;
	}

	/** Bytes pushed and not popped, the most there have been, and the pushes that did not fit. */
	[[nodiscard]] SIZE_T GetUsedBytes() const
	{
		return Used;
	}
	[[nodiscard]] SIZE_T GetPeakBytes() const
	{
		return PeakUsed;
	}
	[[nodiscard]] int32 GetNumOverflows() const
	{
		return NumOverflows;
	}

	/** Open marks. */
	[[nodiscard]] int32 GetNumMarks() const
	{
		return NumMarks;
	}

private:
	FScratchpad();

	uint8* Base = nullptr;
	SIZE_T Used = 0;
	SIZE_T PeakUsed = 0;
	int32 NumOverflows = 0;
	int32 NumMarks = 0;
	bool bOnChip = false;

	friend class FScratchpadMark;
};

/** Gives back, when it goes out of scope, everything pushed on the scratchpad after it was made (FMemMark's twin). */
class CORE_API FScratchpadMark
{
public:
	FScratchpadMark();
	~FScratchpadMark();
	FScratchpadMark(const FScratchpadMark&) = delete;
	FScratchpadMark& operator=(const FScratchpadMark&) = delete;

private:
	SIZE_T Used;
};

/**
 * A container allocator on the scratchpad (TMemStackAllocator's twin): TArray<T, TScratchpadAllocator<>> for a
 * temporary array inside an FScratchpadMark (and an FMemMark, where it overflows). The block at the scratchpad's top
 * grows in place; another one pushes a new block and copies. Declare the marks before the container.
 */
template <uint32 Alignment = DEFAULT_ALIGNMENT>
class TScratchpadAllocator
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

		~ForElementType()
		{
			if (Data != nullptr && !bOnScratchpad)
			{
				FMemStack::Get().RemoveContainerAllocation();
			}
		}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return Data;
		}

		void ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement,
			uint32 AlignmentOfElement = DEFAULT_ALIGNMENT)
		{
			FScratchpad& Scratchpad = FScratchpad::Get();
			ElementType* OldData = Data;
			const bool bOldOnScratchpad = bOnScratchpad;
			if (NumElements > 0)
			{
				const SIZE_T NewBytes = SIZE_T(NumElements) * NumBytesPerElement;
				if (OldData != nullptr && bOldOnScratchpad &&
					Scratchpad.GrowTop(reinterpret_cast<const uint8*>(OldData), AllocatedBytes, NewBytes))
				{
					AllocatedBytes = NewBytes;
					return;
				}
				SIZE_T BlockAlignment = Alignment > AlignmentOfElement ? Alignment : AlignmentOfElement;
				BlockAlignment = BlockAlignment > alignof(ElementType) ? BlockAlignment : alignof(ElementType);
				BlockAlignment = BlockAlignment > 16 ? BlockAlignment : 16;
				Data = reinterpret_cast<ElementType*>(Scratchpad.PushBytes(NewBytes, BlockAlignment));
				bOnScratchpad = Scratchpad.Contains(Data);
				AllocatedBytes = NewBytes;
				if (OldData != nullptr)
				{
					const SizeType NumToCopy = PreviousNumElements < NumElements ? PreviousNumElements : NumElements;
					FMemory::Memcpy(Data, OldData, SIZE_T(NumToCopy) * NumBytesPerElement);
				}
				// The frame's stack counts the containers holding its memory (FMemStack::EndFrame checks them).
				if ((OldData == nullptr || bOldOnScratchpad) && !bOnScratchpad)
				{
					FMemStack::Get().AddContainerAllocation();
				}
				else if (OldData != nullptr && !bOldOnScratchpad && bOnScratchpad)
				{
					FMemStack::Get().RemoveContainerAllocation();
				}
			}
			else if (OldData != nullptr)
			{
				Data = nullptr;
				AllocatedBytes = 0;
				if (!bOldOnScratchpad)
				{
					FMemStack::Get().RemoveContainerAllocation();
				}
				bOnScratchpad = false;
			}
		}

		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, false);
		}
		FORCEINLINE SizeType CalculateSlackShrink(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, false);
		}
		FORCEINLINE SizeType CalculateSlackGrow(
			SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, false);
		}

		SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return SIZE_T(NumAllocatedElements) * NumBytesPerElement;
		}

		bool HasAllocation() const
		{
			return Data != nullptr;
		}

		SizeType GetInitialCapacity() const
		{
			return 0;
		}

	private:
		ElementType* Data = nullptr;
		SIZE_T AllocatedBytes = 0;
		bool bOnScratchpad = false;
	};

	typedef void ForAnyElementType;
};
