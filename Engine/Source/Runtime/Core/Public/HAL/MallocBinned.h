#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "HAL/MemoryBase.h"

/**
 * The engine's allocator, GMalloc on every platform (UE: FMallocBinned; Docs/PLANS/ps2-shipping.md N17), so the PC
 * allocates the way the EE does.
 *
 * - Small blocks (up to MaxSmallBlockSize after alignment) come from size classes BlockGranularity bytes apart. A class
 *   takes PageSize pages from an arena reserved from the system heap when the allocator is made (a platform's
 *   FPlatformProperties::SmallBlockArenaSize for GMalloc); a page is carved front to back, and a freed block goes on
 *   its page's free list. Allocating and freeing are O(1): the class keeps a list of its pages with room, a pointer's
 *   page is its offset in the arena, and a page that empties goes back to the arena for any class.
 * - Alignment: 16 bytes by default; up to MaxSmallBlockSize a larger power of two picks a class whose size is a
 *   multiple of it (the pages are page aligned), beyond that the block is large.
 * - Large blocks, and small ones when the arena is full (counted: ArenaOverflows), come from the system heap (malloc)
 *   with a small header before the block (the original pointer, the size, the tag).
 * - Stats: current, peak and live bytes, the allocations since start-up, the arena's pages, and each size class's
 *   blocks (GetSizeClassStats).
 * - With bTrackTags (GMalloc), every block is charged to its LLM tag (FLowLevelMemTracker; HAL/LowLevelMemTracker.h),
 *   a byte per BlockGranularity of the arena and a field of the large header.
 * - Deterministic: the same calls get the same blocks. A spin lock guards it (the engine allocates from one thread).
 */
class CORE_API FMallocBinned final : public FMalloc
{
public:
	static constexpr uint32 PageSize = 4096;
	static constexpr uint32 BlockGranularity = 16;
	static constexpr uint32 MaxSmallBlockSize = 1024;
	static constexpr int32 NumSizeClasses = int32(MaxSmallBlockSize / BlockGranularity);

	/** Reserves ArenaSize bytes (whole pages) of the system heap for the small blocks; bTrackTags charges LLM tags. */
	FMallocBinned(SIZE_T ArenaSize, bool bInTrackTags);
	/** Gives the arena back (a private allocator of a test; GMalloc lives until the process ends). */
	virtual ~FMallocBinned() override;
	FMallocBinned(const FMallocBinned&) = delete;
	FMallocBinned& operator=(const FMallocBinned&) = delete;

	virtual void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) override;
	virtual void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) override;
	virtual void Free(void* Original) override;
	virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override;
	virtual bool GetAllocationSize(void* Original, SIZE_T& SizeOut) override;
	virtual FMallocUsage GetUsage() const override;
	virtual int32 GetSizeClassStats(FMallocSizeClassStats* OutClasses, int32 MaxClasses) const override;

	virtual const TCHAR* GetDescriptiveName() override
	{
		return TEXT("Binned");
	}

	/** True when Ptr lies in the small-block arena. */
	[[nodiscard]] bool IsSmallBlock(const void* Ptr) const
	{
		return reinterpret_cast<const uint8*>(Ptr) >= ArenaBase && reinterpret_cast<const uint8*>(Ptr) < ArenaEnd;
	}

private:
	/** A page of the arena. */
	struct FPoolPage
	{
		/** The page's freed blocks, linked through their first word. */
		void* FirstFree = nullptr;
		/** Blocks handed out now. */
		uint16 NumUsed = 0;
		/** Blocks carved from the page's start so far; the rest of the page has never been handed out. */
		uint16 NumCarved = 0;
		/** Its size class; NoSizeClass while the page is free. */
		uint8 SizeClass = 0xFF;
		/** The class's list of pages with room, or the arena's free pages (Next only): page indices. */
		int32 Prev = -1;
		int32 Next = -1;
	};

	struct FSizeClass
	{
		uint32 BlockSize = 0;
		uint32 BlocksPerPage = 0;
		/** The first page with a free block (-1: none). */
		int32 FirstPartialPage = -1;
		uint32 CurrentBlocks = 0;
		uint32 PeakBlocks = 0;
		uint32 NumPages = 0;
	};

	/** Allocates a block; OutSize is its usable size (the class's block size, or the request for a large block). */
	void* AllocateLocked(SIZE_T Count, uint32 Alignment, ELLMTag Tag, SIZE_T& OutSize);
	/** Frees a block; OutTag / OutSize say what it was charged. False when Original is not a live block. */
	bool FreeLocked(void* Original, ELLMTag& OutTag, SIZE_T& OutSize);
	void* AllocateSmallLocked(int32 ClassIndex, ELLMTag Tag);
	void* AllocateLargeLocked(SIZE_T Size, uint32 Alignment, ELLMTag Tag);
	[[nodiscard]] SIZE_T GetUsableSizeLocked(void* Original) const;
	int32 TakeFreePage();
	void LinkPartialPage(FSizeClass& Class, int32 PageIndex);
	void UnlinkPartialPage(FSizeClass& Class, int32 PageIndex);
	void Lock();
	void Unlock();

	uint8* ArenaBase = nullptr;
	uint8* ArenaEnd = nullptr;
	/** What the system heap returned for the arena (the arena is its page-aligned part). */
	void* ArenaAllocation = nullptr;
	FPoolPage* Pages = nullptr;
	/** The tag of each BlockGranularity bytes of the arena (bTrackTags). */
	uint8* ArenaTags = nullptr;
	int32 NumPages = 0;
	/** Pages below this index have been used; the ones above were never touched. */
	int32 NumTouchedPages = 0;
	/** The arena's freed pages, linked through Next. */
	int32 FirstFreePage = -1;
	int32 PagesInUse = 0;
	int32 PeakPagesInUse = 0;
	FSizeClass SizeClasses[NumSizeClasses];

	uint64 CurrentBytes = 0;
	uint64 PeakBytes = 0;
	uint64 NumAllocations = 0;
	uint64 TotalAllocations = 0;
	uint64 ArenaOverflows = 0;
	volatile int32 LockValue = 0;
	bool bTrackTags = false;
};
