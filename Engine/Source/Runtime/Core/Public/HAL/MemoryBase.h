#pragma once

#include "CoreTypes.h"

enum
{
	/** Let the allocator pick its natural alignment (16 bytes on every Leon platform). */
	DEFAULT_ALIGNMENT = 0,

	/** Smallest alignment an allocator returns. */
	MIN_ALIGNMENT = 8
};

/** Bytes handed out by an allocator (Leon extension of UE's FMalloc stats). */
struct FMallocUsage
{
	/** Bytes currently allocated (usable size, allocator overhead excluded). */
	uint64 CurrentBytes = 0;

	/** Highest CurrentBytes since start-up. */
	uint64 PeakBytes = 0;

	/** Live allocations. */
	uint64 NumAllocations = 0;

	/** Allocations made since start-up: every Malloc, and every Realloc that moved its block (per frame: heap churn).
	 */
	uint64 TotalAllocations = 0;

	/** The small-block arena: its size, the bytes of its pages in use now and at most (0 without one). */
	uint64 ArenaBytes = 0;
	uint64 ArenaUsedBytes = 0;
	uint64 ArenaPeakBytes = 0;

	/** Small blocks that found the arena full and went to the system heap. */
	uint64 ArenaOverflows = 0;
};

/** One size class of a small-block allocator (Leon; UE: FMallocBinned's pool table stats). */
struct FMallocSizeClassStats
{
	/** The block size in bytes. */
	uint32 BlockSize = 0;
	/** Blocks in use now and at most. */
	uint32 CurrentBlocks = 0;
	uint32 PeakBlocks = 0;
	/** Pages the class holds now. */
	uint32 NumPages = 0;
};

/** Allocator interface behind FMemory (UE: FMalloc). */
class CORE_API FMalloc
{
public:
	virtual ~FMalloc() = default;

	virtual void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) = 0;
	virtual void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) = 0;
	virtual void Free(void* Original) = 0;

	/** Size the allocator would really hand out for a request (containers grow into the slack). */
	virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 /*Alignment*/)
	{
		return Count;
	}

	/** Usable size of an allocation; false when the allocator cannot tell. */
	virtual bool GetAllocationSize(void* /*Original*/, SIZE_T& /*SizeOut*/)
	{
		return false;
	}

	virtual FMallocUsage GetUsage() const
	{
		return {};
	}

	/** Fills up to MaxClasses entries of OutClasses with the size classes; returns how many there are (Leon). */
	virtual int32 GetSizeClassStats(FMallocSizeClassStats* /*OutClasses*/, int32 /*MaxClasses*/) const
	{
		return 0;
	}

	virtual const TCHAR* GetDescriptiveName()
	{
		return TEXT("Unspecified allocator");
	}
};

/** The global allocator; created on first use by FMemory (UE: GMalloc). */
extern CORE_API FMalloc* GMalloc;
