#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "CoreTypes.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"

/**
 * A stack of memory for temporaries (UE: FMemStackBase; Docs/PLANS/ps2-shipping.md N17): PushBytes bumps a pointer
 * through chunks taken from GMalloc (under a tag of their own), an FMemMark gives back everything pushed after it, and
 * nothing is freed one by one.
 *
 * - The chunks stay with the stack once popped, so a stack that reaches the same depth every frame stops allocating
 *   from the heap after its first frames; Flush gives them back to GMalloc. A request larger than the chunk size gets a
 *   chunk of its own, kept too.
 * - A container (TMemStackAllocator) that holds memory of the stack is counted: a mark that pops while a container
 *   allocated inside it still holds its memory is an error, as is a frame that ends with one (EndFrame).
 * - One thread: FMemStack::Get() is the game thread's.
 */
class CORE_API FMemStackBase
{
public:
	/** The chunk size of the frame's stack (FMemStack). */
	static constexpr int32 DefaultChunkSize = 16 * 1024;

	explicit FMemStackBase(int32 InChunkSize = DefaultChunkSize, ELLMTag InChunkTag = ELLMTag::Temporary);
	/** Frees the chunks; no mark may be open. */
	~FMemStackBase();
	FMemStackBase(const FMemStackBase&) = delete;
	FMemStackBase& operator=(const FMemStackBase&) = delete;

	/** AllocSize bytes aligned to Alignment (a power of two), valid until the enclosing mark pops (UE). */
	FORCEINLINE uint8* PushBytes(SIZE_T AllocSize, SIZE_T Alignment)
	{
		const UPTRINT Result =
			(reinterpret_cast<UPTRINT>(Top) + (Alignment - 1)) & ~static_cast<UPTRINT>(Alignment - 1);
		if (Top != nullptr && Result + AllocSize <= reinterpret_cast<UPTRINT>(End))
		{
			Top = reinterpret_cast<uint8*>(Result + AllocSize);
			return reinterpret_cast<uint8*>(Result);
		}
		return PushBytesInNewChunk(AllocSize, Alignment);
	}

	/** PushBytes (UE). */
	FORCEINLINE void* Alloc(SIZE_T AllocSize, SIZE_T Alignment)
	{
		return PushBytes(AllocSize, Alignment);
	}

	/** Nothing is pushed (UE). */
	[[nodiscard]] bool IsEmpty() const
	{
		return TopChunk == nullptr;
	}

	/** Pops everything and gives every chunk back to GMalloc; no mark may be open (UE). */
	void Flush();

	/**
	 * The end of a frame (Leon): checks that no mark is open and no container holds memory of the stack (a use after
	 * the frame), then pops everything. The chunks stay for the next frame.
	 */
	void EndFrame();

	/** Bytes pushed and not popped: the top chunk's used part and the whole of the chunks under it (UE). */
	[[nodiscard]] SIZE_T GetByteCount() const;

	/** The most GetByteCount has been at a mark's pop or a frame's end (Leon). */
	[[nodiscard]] SIZE_T GetPeakByteCount() const
	{
		return PeakByteCount;
	}

	/** Bytes of the chunks the stack holds, in use or kept (Leon). */
	[[nodiscard]] SIZE_T GetChunkBytes() const
	{
		return ChunkBytes;
	}

	/** Chunks taken from GMalloc since the stack was made (Leon): constant once the stack has warmed up. */
	[[nodiscard]] int32 GetNumChunkAllocations() const
	{
		return NumChunkAllocations;
	}

	/** Open marks (UE). */
	[[nodiscard]] int32 GetNumMarks() const
	{
		return NumMarks;
	}

	/** Whether Pointer lies in a chunk in use (UE). */
	[[nodiscard]] bool ContainsPointer(const void* Pointer) const;

	/** TMemStackAllocator's count of the containers holding memory of the stack (Leon). */
	void AddContainerAllocation()
	{
		++NumContainerAllocations;
	}
	void RemoveContainerAllocation()
	{
		checkSlow(NumContainerAllocations > 0);
		--NumContainerAllocations;
	}
	[[nodiscard]] int32 GetNumContainerAllocations() const
	{
		return NumContainerAllocations;
	}

private:
	/** A chunk's header; its data follows, ChunkHeaderSize bytes after the header's start. */
	struct FChunk
	{
		FChunk* Next;
		SIZE_T DataSize;
	};
	static constexpr SIZE_T ChunkHeaderSize = 16;
	static_assert(sizeof(FChunk) <= ChunkHeaderSize, "FChunk header");

	static uint8* GetChunkData(FChunk* Chunk)
	{
		return reinterpret_cast<uint8*>(Chunk) + ChunkHeaderSize;
	}

	uint8* PushBytesInNewChunk(SIZE_T AllocSize, SIZE_T Alignment);
	/** Pops back to Top in Chunk (null: everything); the chunks above go to the kept ones. */
	void PopTo(FChunk* Chunk, uint8* InTop);
	void UpdatePeak();

	uint8* Top = nullptr;
	uint8* End = nullptr;
	/** The chunks in use, the top one first. */
	FChunk* TopChunk = nullptr;
	/** The chunks popped and kept for later. */
	FChunk* KeptChunks = nullptr;
	SIZE_T ChunkBytes = 0;
	SIZE_T PeakByteCount = 0;
	int32 ChunkSize;
	int32 NumMarks = 0;
	int32 NumContainerAllocations = 0;
	int32 NumChunkAllocations = 0;
	ELLMTag ChunkTag;

	friend class FMemMark;
};

/** The game thread's stack of the frame's temporaries (UE: FMemStack); the engine loop ends its frame (EndFrame). */
class CORE_API FMemStack : public FMemStackBase
{
public:
	/** The game thread's stack (UE: TThreadSingleton<FMemStack>::Get(); Leon's engine has one thread). */
	static FMemStack& Get();

private:
	FMemStack() = default;
};

/** Gives back, when it goes out of scope, everything pushed on a stack after it was made (UE: FMemMark). */
class CORE_API FMemMark
{
public:
	explicit FMemMark(FMemStackBase& InMem);
	~FMemMark()
	{
		Pop();
	}
	FMemMark(const FMemMark&) = delete;
	FMemMark& operator=(const FMemMark&) = delete;

	/** Pops now (UE); the destructor then does nothing. */
	void Pop();

private:
	FMemStackBase& Mem;
	uint8* Top;
	FMemStackBase::FChunk* SavedChunk;
	int32 NumContainerAllocations;
	bool bPopped = false;
};

/** Constructs a T on the stack: `new (FMemStack::Get()) T(...)`, freed by the enclosing mark (UE). */
inline void* operator new(size_t Size, FMemStackBase& Mem, int32 Count = 1, int32 Align = DEFAULT_ALIGNMENT)
{
	return Mem.PushBytes(Size * SIZE_T(Count), SIZE_T(Align > 16 ? Align : 16));
}

/** The matching placement delete (never called: no exceptions). */
inline void operator delete(void* /*Ptr*/, FMemStackBase& /*Mem*/, int32 /*Count*/, int32 /*Align*/)
{
}

/**
 * A container allocator on the frame's stack (UE: TMemStackAllocator): TArray<T, TMemStackAllocator<>> for a
 * temporary array inside an FMemMark scope. Growing pushes a new block and copies (the old one waits for the mark);
 * freeing does nothing. Declare the mark before the container, so the container goes first.
 */
template <uint32 Alignment = DEFAULT_ALIGNMENT>
class TMemStackAllocator
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
			if (Data != nullptr)
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
			FMemStack& Stack = FMemStack::Get();
			ElementType* OldData = Data;
			if (NumElements > 0)
			{
				SIZE_T BlockAlignment = Alignment > AlignmentOfElement ? Alignment : AlignmentOfElement;
				BlockAlignment = BlockAlignment > alignof(ElementType) ? BlockAlignment : alignof(ElementType);
				BlockAlignment = BlockAlignment > 16 ? BlockAlignment : 16;
				Data = reinterpret_cast<ElementType*>(
					Stack.PushBytes(SIZE_T(NumElements) * NumBytesPerElement, BlockAlignment));
				if (OldData != nullptr)
				{
					const SizeType NumToCopy = PreviousNumElements < NumElements ? PreviousNumElements : NumElements;
					FMemory::Memcpy(Data, OldData, SIZE_T(NumToCopy) * NumBytesPerElement);
				}
				else
				{
					Stack.AddContainerAllocation();
				}
			}
			else if (OldData != nullptr)
			{
				Data = nullptr;
				Stack.RemoveContainerAllocation();
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
	};

	typedef void ForAnyElementType;
};
