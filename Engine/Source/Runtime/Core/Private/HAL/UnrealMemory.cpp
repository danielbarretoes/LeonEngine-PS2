#include "HAL/UnrealMemory.h"

#include "HAL/MallocAnsi.h"

#include <new>

FMalloc* GMalloc = nullptr;

namespace
{
	/** Creates GMalloc on first use. The allocator is never destroyed: statics may free memory during exit. */
	FORCENOINLINE void CreateGMalloc()
	{
		alignas(FMallocAnsi) static uint8 Storage[sizeof(FMallocAnsi)];
		GMalloc = new (Storage) FMallocAnsi();
	}

	FORCEINLINE FMalloc& GetGMalloc()
	{
		if (UNLIKELY(GMalloc == nullptr))
		{
			CreateGMalloc();
		}
		return *GMalloc;
	}
} // namespace

void* FMemory::Malloc(SIZE_T Count, uint32 Alignment)
{
	return GetGMalloc().Malloc(Count, Alignment);
}

void* FMemory::Realloc(void* Original, SIZE_T Count, uint32 Alignment)
{
	return GetGMalloc().Realloc(Original, Count, Alignment);
}

void FMemory::Free(void* Original)
{
	if (Original)
	{
		GetGMalloc().Free(Original);
	}
}

void* FMemory::MallocZeroed(SIZE_T Count, uint32 Alignment)
{
	void* Memory = Malloc(Count, Alignment);
	if (Memory)
	{
		Memzero(Memory, Count);
	}
	return Memory;
}

SIZE_T FMemory::GetAllocSize(void* Original)
{
	SIZE_T Size = 0;
	return (Original && GetGMalloc().GetAllocationSize(Original, Size)) ? Size : 0;
}

SIZE_T FMemory::QuantizeSize(SIZE_T Count, uint32 Alignment)
{
	return GetGMalloc().QuantizeSize(Count, Alignment);
}

FMallocUsage FMemory::GetUsage()
{
	return GetGMalloc().GetUsage();
}
