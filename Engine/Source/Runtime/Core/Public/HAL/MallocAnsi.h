#pragma once

#include "HAL/MemoryBase.h"

/**
 * C runtime allocator with usage tracking (UE: FMallocAnsi). Windows keeps the requested size in a small header;
 * other platforms ask the C library (malloc_usable_size), so the EE heap pays no per-block overhead.
 */
class CORE_API FMallocAnsi final : public FMalloc
{
public:
	virtual void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) override;
	virtual void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT) override;
	virtual void Free(void* Original) override;
	virtual bool GetAllocationSize(void* Original, SIZE_T& SizeOut) override;
	virtual FMallocUsage GetUsage() const override;

	virtual const TCHAR* GetDescriptiveName() override
	{
		return TEXT("Ansi");
	}

private:
	void TrackAlloc(SIZE_T Size);
	void TrackFree(SIZE_T Size);

	volatile int64 CurrentBytes = 0;
	volatile int64 PeakBytes = 0;
	volatile int64 NumAllocations = 0;
};
