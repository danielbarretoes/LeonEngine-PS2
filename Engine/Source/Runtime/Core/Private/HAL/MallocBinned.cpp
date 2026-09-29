#include "HAL/MallocBinned.h"

#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AssertionMacros.h"

#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
	constexpr uint32 PageShift = 12;
	static_assert((1u << PageShift) == FMallocBinned::PageSize, "PageShift");
	constexpr uint32 GranuleShift = 4;
	static_assert((1u << GranuleShift) == FMallocBinned::BlockGranularity, "GranuleShift");
	constexpr uint8 NoSizeClass = 0xFF;
	constexpr int32 NoPage = -1;

	/** Every block is at least this aligned. */
	constexpr uint32 MinBlockAlignment = FMallocBinned::BlockGranularity;

	/** A block from the system heap; the header sits right before the block. */
	struct FLargeBlockHeader
	{
		void* Original;
		SIZE_T Size;
		uint32 Tag;
	};

	[[noreturn]] FORCENOINLINE void OutOfMemory(SIZE_T Count, uint32 Alignment)
	{
		FPlatformMisc::LowLevelOutputDebugStringf("FMallocBinned: out of memory allocating %llu bytes (alignment %u)\n",
			static_cast<unsigned long long>(Count), Alignment);
		FPlatformMisc::RequestExit(true);
		std::abort();
	}

	FORCEINLINE uint32 EffectiveAlignment(uint32 Alignment)
	{
		return Alignment < MinBlockAlignment ? MinBlockAlignment : Alignment;
	}

	/** The size class of a request, or -1 when the block is large. */
	FORCEINLINE int32 GetSizeClass(SIZE_T Count, uint32 Alignment)
	{
		if (Alignment > FMallocBinned::MaxSmallBlockSize)
		{
			return -1;
		}
		const SIZE_T Request = Count ? Count : 1;
		const SIZE_T Size = (Request + Alignment - 1) & ~SIZE_T(Alignment - 1);
		return Size <= FMallocBinned::MaxSmallBlockSize ? int32(Size >> GranuleShift) - 1 : -1;
	}

	FORCEINLINE FLargeBlockHeader* GetLargeHeader(void* Ptr)
	{
		return reinterpret_cast<FLargeBlockHeader*>(static_cast<uint8*>(Ptr) - sizeof(FLargeBlockHeader));
	}
} // namespace

FMallocBinned::FMallocBinned(SIZE_T ArenaSize, bool bInTrackTags)
	: bTrackTags(bInTrackTags && FLowLevelMemTracker::IsEnabled())
{
	for (int32 Index = 0; Index < NumSizeClasses; ++Index)
	{
		FSizeClass& Class = SizeClasses[Index];
		Class.BlockSize = uint32(Index + 1) * BlockGranularity;
		Class.BlocksPerPage = PageSize / Class.BlockSize;
	}
	NumPages = int32(ArenaSize / PageSize);
	if (NumPages <= 0)
	{
		NumPages = 0;
		return;
	}
	ArenaAllocation = std::malloc(SIZE_T(NumPages) * PageSize + PageSize);
	Pages = static_cast<FPoolPage*>(std::malloc(sizeof(FPoolPage) * SIZE_T(NumPages)));
	const SIZE_T NumGranules = SIZE_T(NumPages) * (PageSize / BlockGranularity);
	ArenaTags = bTrackTags ? static_cast<uint8*>(std::malloc(NumGranules)) : nullptr;
	if (ArenaAllocation == nullptr || Pages == nullptr || (bTrackTags && ArenaTags == nullptr))
	{
		OutOfMemory(SIZE_T(NumPages) * PageSize, PageSize);
	}
	for (int32 Index = 0; Index < NumPages; ++Index)
	{
		new (&Pages[Index]) FPoolPage();
	}
	ArenaBase = reinterpret_cast<uint8*>(
		(reinterpret_cast<UPTRINT>(ArenaAllocation) + (PageSize - 1)) & ~static_cast<UPTRINT>(PageSize - 1));
	ArenaEnd = ArenaBase + SIZE_T(NumPages) * PageSize;
}

FMallocBinned::~FMallocBinned()
{
	std::free(ArenaAllocation);
	std::free(Pages);
	std::free(ArenaTags);
}

void FMallocBinned::Lock()
{
	while (FPlatformAtomics::InterlockedCompareExchange(&LockValue, 1, 0) != 0)
	{
	}
}

void FMallocBinned::Unlock()
{
	FPlatformAtomics::InterlockedExchange(&LockValue, 0);
}

int32 FMallocBinned::TakeFreePage()
{
	int32 PageIndex = FirstFreePage;
	if (PageIndex != NoPage)
	{
		FirstFreePage = Pages[PageIndex].Next;
	}
	else if (NumTouchedPages < NumPages)
	{
		PageIndex = NumTouchedPages++;
	}
	else
	{
		return NoPage;
	}
	++PagesInUse;
	PeakPagesInUse = PagesInUse > PeakPagesInUse ? PagesInUse : PeakPagesInUse;
	return PageIndex;
}

void FMallocBinned::LinkPartialPage(FSizeClass& Class, int32 PageIndex)
{
	FPoolPage& Page = Pages[PageIndex];
	Page.Prev = NoPage;
	Page.Next = Class.FirstPartialPage;
	if (Class.FirstPartialPage != NoPage)
	{
		Pages[Class.FirstPartialPage].Prev = PageIndex;
	}
	Class.FirstPartialPage = PageIndex;
}

void FMallocBinned::UnlinkPartialPage(FSizeClass& Class, int32 PageIndex)
{
	FPoolPage& Page = Pages[PageIndex];
	if (Page.Prev != NoPage)
	{
		Pages[Page.Prev].Next = Page.Next;
	}
	else
	{
		Class.FirstPartialPage = Page.Next;
	}
	if (Page.Next != NoPage)
	{
		Pages[Page.Next].Prev = Page.Prev;
	}
	Page.Prev = NoPage;
	Page.Next = NoPage;
}

void* FMallocBinned::AllocateSmallLocked(int32 ClassIndex, ELLMTag Tag)
{
	FSizeClass& Class = SizeClasses[ClassIndex];
	int32 PageIndex = Class.FirstPartialPage;
	if (PageIndex == NoPage)
	{
		PageIndex = TakeFreePage();
		if (PageIndex == NoPage)
		{
			return nullptr;
		}
		FPoolPage& NewPage = Pages[PageIndex];
		NewPage.FirstFree = nullptr;
		NewPage.NumUsed = 0;
		NewPage.NumCarved = 0;
		NewPage.SizeClass = uint8(ClassIndex);
		LinkPartialPage(Class, PageIndex);
		++Class.NumPages;
	}
	FPoolPage& Page = Pages[PageIndex];
	uint8* Block;
	if (Page.FirstFree != nullptr)
	{
		Block = static_cast<uint8*>(Page.FirstFree);
		Page.FirstFree = *reinterpret_cast<void**>(Block);
	}
	else
	{
		Block = ArenaBase + (SIZE_T(PageIndex) << PageShift) + SIZE_T(Page.NumCarved) * Class.BlockSize;
		++Page.NumCarved;
	}
	if (++Page.NumUsed == Class.BlocksPerPage)
	{
		UnlinkPartialPage(Class, PageIndex);
	}
	++Class.CurrentBlocks;
	Class.PeakBlocks = Class.CurrentBlocks > Class.PeakBlocks ? Class.CurrentBlocks : Class.PeakBlocks;
	if (ArenaTags != nullptr)
	{
		ArenaTags[SIZE_T(Block - ArenaBase) >> GranuleShift] = uint8(Tag);
	}
	return Block;
}

void* FMallocBinned::AllocateLargeLocked(SIZE_T Size, uint32 Alignment, ELLMTag Tag)
{
	const SIZE_T Total = Size + Alignment + sizeof(FLargeBlockHeader);
	if (Total < Size)
	{
		OutOfMemory(Size, Alignment);
	}
	void* Original = std::malloc(Total);
	if (UNLIKELY(Original == nullptr))
	{
		OutOfMemory(Size, Alignment);
	}
	const UPTRINT First = reinterpret_cast<UPTRINT>(Original) + sizeof(FLargeBlockHeader);
	void* Result = reinterpret_cast<void*>((First + (Alignment - 1)) & ~static_cast<UPTRINT>(Alignment - 1));
	FLargeBlockHeader* Header = GetLargeHeader(Result);
	Header->Original = Original;
	Header->Size = Size;
	Header->Tag = uint32(Tag);
	return Result;
}

void* FMallocBinned::AllocateLocked(SIZE_T Count, uint32 Alignment, ELLMTag Tag, SIZE_T& OutSize)
{
	void* Result = nullptr;
	const int32 ClassIndex = GetSizeClass(Count, Alignment);
	if (ClassIndex >= 0)
	{
		Result = AllocateSmallLocked(ClassIndex, Tag);
		if (Result != nullptr)
		{
			OutSize = SizeClasses[ClassIndex].BlockSize;
		}
		else
		{
			// The arena is full: the system heap takes it, at the class's size.
			++ArenaOverflows;
			OutSize = SizeClasses[ClassIndex].BlockSize;
			Result = AllocateLargeLocked(OutSize, Alignment, Tag);
		}
	}
	else
	{
		OutSize = Count ? Count : 1;
		Result = AllocateLargeLocked(OutSize, Alignment, Tag);
	}
	CurrentBytes += OutSize;
	PeakBytes = CurrentBytes > PeakBytes ? CurrentBytes : PeakBytes;
	++NumAllocations;
	++TotalAllocations;
	return Result;
}

SIZE_T FMallocBinned::GetUsableSizeLocked(void* Original) const
{
	if (IsSmallBlock(Original))
	{
		const FPoolPage& Page = Pages[SIZE_T(static_cast<uint8*>(Original) - ArenaBase) >> PageShift];
		return SizeClasses[Page.SizeClass].BlockSize;
	}
	return GetLargeHeader(Original)->Size;
}

bool FMallocBinned::FreeLocked(void* Original, ELLMTag& OutTag, SIZE_T& OutSize)
{
	uint8* Block = static_cast<uint8*>(Original);
	if (IsSmallBlock(Block))
	{
		const int32 PageIndex = int32(SIZE_T(Block - ArenaBase) >> PageShift);
		FPoolPage& Page = Pages[PageIndex];
		if (Page.SizeClass == NoSizeClass || Page.NumUsed == 0)
		{
			return false;
		}
		FSizeClass& Class = SizeClasses[Page.SizeClass];
#if DO_GUARD_SLOW
		if ((SIZE_T(Block - ArenaBase) & (PageSize - 1)) % Class.BlockSize != 0)
		{
			return false;
		}
#endif
		OutSize = Class.BlockSize;
		OutTag =
			ArenaTags != nullptr ? ELLMTag(ArenaTags[SIZE_T(Block - ArenaBase) >> GranuleShift]) : ELLMTag::EngineMisc;
		if (Page.NumUsed == Class.BlocksPerPage)
		{
			LinkPartialPage(Class, PageIndex);
		}
		*reinterpret_cast<void**>(Block) = Page.FirstFree;
		Page.FirstFree = Block;
		--Class.CurrentBlocks;
		if (--Page.NumUsed == 0)
		{
			// Empty: the page goes back to the arena, for any class.
			UnlinkPartialPage(Class, PageIndex);
			--Class.NumPages;
			Page.SizeClass = NoSizeClass;
			Page.FirstFree = nullptr;
			Page.Next = FirstFreePage;
			FirstFreePage = PageIndex;
			--PagesInUse;
		}
	}
	else
	{
		FLargeBlockHeader* Header = GetLargeHeader(Block);
		OutSize = Header->Size;
		OutTag = ELLMTag(Header->Tag);
		std::free(Header->Original);
	}
	CurrentBytes -= OutSize;
	--NumAllocations;
	return true;
}

void* FMallocBinned::Malloc(SIZE_T Count, uint32 Alignment)
{
	Alignment = EffectiveAlignment(Alignment);
	const ELLMTag Tag = FLowLevelMemTracker::GetActiveTag();
	SIZE_T Size = 0;
	bool bReport = false;
	Lock();
	void* Result = AllocateLocked(Count, Alignment, Tag, Size);
	if (bTrackTags)
	{
		bReport = FLowLevelMemTracker::OnAlloc(Tag, Size);
	}
	Unlock();
	if (UNLIKELY(bReport))
	{
		FLowLevelMemTracker::ReportBudgets(Tag);
	}
	return Result;
}

void* FMallocBinned::Realloc(void* Original, SIZE_T Count, uint32 Alignment)
{
	if (Original == nullptr)
	{
		return Malloc(Count, Alignment);
	}
	if (Count == 0)
	{
		Free(Original);
		return nullptr;
	}
	Alignment = EffectiveAlignment(Alignment);
	Lock();
	const SIZE_T OldSize = GetUsableSizeLocked(Original);
	const bool bAligned = (reinterpret_cast<UPTRINT>(Original) & (Alignment - 1)) == 0;
	bool bKeep = false;
	if (bAligned)
	{
		if (IsSmallBlock(Original))
		{
			// The same class: the block already fits.
			const int32 ClassIndex = GetSizeClass(Count, Alignment);
			bKeep = ClassIndex >= 0 && SizeClasses[ClassIndex].BlockSize == OldSize;
		}
		else
		{
			// A large block shrinking by less than half stays.
			bKeep = GetSizeClass(Count, Alignment) < 0 && Count <= OldSize && Count > OldSize / 2;
		}
	}
	Unlock();
	if (bKeep)
	{
		return Original;
	}
	void* Result = Malloc(Count, Alignment);
	std::memcpy(Result, Original, OldSize < Count ? OldSize : Count);
	Free(Original);
	return Result;
}

void FMallocBinned::Free(void* Original)
{
	if (Original == nullptr)
	{
		return;
	}
	ELLMTag Tag = ELLMTag::EngineMisc;
	SIZE_T Size = 0;
	Lock();
	const bool bFreed = FreeLocked(Original, Tag, Size);
	if (bFreed && bTrackTags)
	{
		FLowLevelMemTracker::OnFree(Tag, Size);
	}
	Unlock();
	// Outside the lock: the error's log allocates.
	checkf(bFreed, "FMallocBinned: %p is not a block (freed twice, or never allocated)", Original);
}

SIZE_T FMallocBinned::QuantizeSize(SIZE_T Count, uint32 Alignment)
{
	const int32 ClassIndex = GetSizeClass(Count, EffectiveAlignment(Alignment));
	return ClassIndex >= 0 ? SIZE_T(SizeClasses[ClassIndex].BlockSize) : Count;
}

bool FMallocBinned::GetAllocationSize(void* Original, SIZE_T& SizeOut)
{
	if (Original == nullptr)
	{
		return false;
	}
	Lock();
	SizeOut = GetUsableSizeLocked(Original);
	Unlock();
	return true;
}

FMallocUsage FMallocBinned::GetUsage() const
{
	FMallocUsage Usage;
	Usage.CurrentBytes = CurrentBytes;
	Usage.PeakBytes = PeakBytes;
	Usage.NumAllocations = NumAllocations;
	Usage.TotalAllocations = TotalAllocations;
	Usage.ArenaBytes = uint64(NumPages) * PageSize;
	Usage.ArenaUsedBytes = uint64(PagesInUse) * PageSize;
	Usage.ArenaPeakBytes = uint64(PeakPagesInUse) * PageSize;
	Usage.ArenaOverflows = ArenaOverflows;
	return Usage;
}

int32 FMallocBinned::GetSizeClassStats(FMallocSizeClassStats* OutClasses, int32 MaxClasses) const
{
	for (int32 Index = 0; Index < NumSizeClasses && Index < MaxClasses; ++Index)
	{
		const FSizeClass& Class = SizeClasses[Index];
		FMallocSizeClassStats& Out = OutClasses[Index];
		Out.BlockSize = Class.BlockSize;
		Out.CurrentBlocks = Class.CurrentBlocks;
		Out.PeakBlocks = Class.PeakBlocks;
		Out.NumPages = Class.NumPages;
	}
	return NumSizeClasses;
}
