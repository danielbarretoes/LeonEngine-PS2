#include "Misc/MemStack.h"

#include "Misc/Build.h"

#if UE_BUILD_DEBUG
namespace
{
	/** Popped memory is filled with this in Debug builds, so a use after the pop shows. */
	constexpr uint8 PoppedFill = 0xDD;
} // namespace
#endif

FMemStackBase::FMemStackBase(int32 InChunkSize, ELLMTag InChunkTag)
	: ChunkSize(InChunkSize > 0 ? InChunkSize : DefaultChunkSize)
	, ChunkTag(InChunkTag)
{
}

FMemStackBase::~FMemStackBase()
{
	check(NumMarks == 0);
	Flush();
}

uint8* FMemStackBase::PushBytesInNewChunk(SIZE_T AllocSize, SIZE_T Alignment)
{
	// The data starts 16-byte aligned: a larger alignment needs the slack.
	const SIZE_T Needed = AllocSize + (Alignment > 16 ? Alignment : 0);
	FChunk* Chunk = nullptr;
	for (FChunk** Link = &KeptChunks; *Link != nullptr; Link = &(*Link)->Next)
	{
		if ((*Link)->DataSize >= Needed)
		{
			Chunk = *Link;
			*Link = Chunk->Next;
			break;
		}
	}
	if (Chunk == nullptr)
	{
		const SIZE_T DataSize = Needed > SIZE_T(ChunkSize) ? Needed : SIZE_T(ChunkSize);
		LLM_SCOPE(ChunkTag);
		Chunk = static_cast<FChunk*>(FMemory::Malloc(ChunkHeaderSize + DataSize, 16));
		Chunk->DataSize = DataSize;
		ChunkBytes += DataSize;
		++NumChunkAllocations;
	}
	Chunk->Next = TopChunk;
	TopChunk = Chunk;
	uint8* Data = GetChunkData(Chunk);
	End = Data + Chunk->DataSize;
	const UPTRINT Result = (reinterpret_cast<UPTRINT>(Data) + (Alignment - 1)) & ~static_cast<UPTRINT>(Alignment - 1);
	Top = reinterpret_cast<uint8*>(Result + AllocSize);
	return reinterpret_cast<uint8*>(Result);
}

SIZE_T FMemStackBase::GetByteCount() const
{
	SIZE_T Count = 0;
	for (FChunk* Chunk = TopChunk; Chunk != nullptr; Chunk = Chunk->Next)
	{
		Count += Chunk == TopChunk ? SIZE_T(Top - GetChunkData(Chunk)) : Chunk->DataSize;
	}
	return Count;
}

bool FMemStackBase::ContainsPointer(const void* Pointer) const
{
	const uint8* Byte = static_cast<const uint8*>(Pointer);
	for (FChunk* Chunk = TopChunk; Chunk != nullptr; Chunk = Chunk->Next)
	{
		if (Byte >= GetChunkData(Chunk) && Byte < GetChunkData(Chunk) + Chunk->DataSize)
		{
			return true;
		}
	}
	return false;
}

void FMemStackBase::UpdatePeak()
{
	const SIZE_T Count = GetByteCount();
	PeakByteCount = Count > PeakByteCount ? Count : PeakByteCount;
}

void FMemStackBase::PopTo(FChunk* Chunk, uint8* InTop)
{
	UpdatePeak();
	bool bPoppedChunks = false;
	while (TopChunk != Chunk)
	{
		checkf(TopChunk != nullptr, "FMemStackBase: popping to a chunk that is not on the stack");
		FChunk* Popped = TopChunk;
		TopChunk = Popped->Next;
#if UE_BUILD_DEBUG
		FMemory::Memset(GetChunkData(Popped), PoppedFill, Popped->DataSize);
#endif
		Popped->Next = KeptChunks;
		KeptChunks = Popped;
		bPoppedChunks = true;
	}
	if (TopChunk == nullptr)
	{
		Top = nullptr;
		End = nullptr;
		return;
	}
	uint8* const ChunkEnd = GetChunkData(TopChunk) + TopChunk->DataSize;
#if UE_BUILD_DEBUG
	// Where the chunk's own pushes ended is not known once a later chunk took over: fill to its end.
	FMemory::Memset(InTop, PoppedFill, SIZE_T((bPoppedChunks ? ChunkEnd : Top) - InTop));
#else
	(void)bPoppedChunks;
#endif
	Top = InTop;
	End = ChunkEnd;
}

void FMemStackBase::Flush()
{
	checkf(NumMarks == 0, "FMemStackBase::Flush with %d mark(s) open", NumMarks);
	PopTo(nullptr, nullptr);
	while (KeptChunks != nullptr)
	{
		FChunk* Chunk = KeptChunks;
		KeptChunks = Chunk->Next;
		ChunkBytes -= Chunk->DataSize;
		FMemory::Free(Chunk);
	}
}

void FMemStackBase::EndFrame()
{
	checkf(NumMarks == 0, "The frame ends with %d FMemMark(s) open on the frame's stack", NumMarks);
	checkf(NumContainerAllocations == 0,
		"%d container(s) with a TMemStackAllocator still hold memory of the frame's stack at the end of the frame",
		NumContainerAllocations);
	PopTo(nullptr, nullptr);
}

FMemStack& FMemStack::Get()
{
	static FMemStack Stack;
	return Stack;
}

FMemMark::FMemMark(FMemStackBase& InMem)
	: Mem(InMem)
	, Top(InMem.Top)
	, SavedChunk(InMem.TopChunk)
	, NumContainerAllocations(InMem.NumContainerAllocations)
{
	++Mem.NumMarks;
}

void FMemMark::Pop()
{
	if (bPopped)
	{
		return;
	}
	bPopped = true;
	checkf(Mem.NumContainerAllocations <= NumContainerAllocations,
		"A container with a TMemStackAllocator outlives the FMemMark it allocated in (%d holding memory, %d before "
		"the mark)",
		Mem.NumContainerAllocations, NumContainerAllocations);
	--Mem.NumMarks;
	Mem.PopTo(SavedChunk, Top);
}
