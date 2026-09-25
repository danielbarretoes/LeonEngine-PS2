#include "HAL/MallocAnsi.h"

#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformMisc.h"
#include "Templates/AlignmentTemplates.h"

#include <cstdlib>
#include <cstring>

#if !PLATFORM_WINDOWS
	#include <malloc.h>
#endif

namespace
{
	/** Every Leon platform's C allocator returns 16-byte aligned blocks. */
	constexpr uint32 NativeAlignment = 16;

	FORCEINLINE uint32 EffectiveAlignment(uint32 Alignment)
	{
		return Alignment < NativeAlignment ? NativeAlignment : Alignment;
	}

	[[noreturn]] FORCENOINLINE void OutOfMemory(SIZE_T Count, uint32 Alignment)
	{
		FPlatformMisc::LowLevelOutputDebugStringf("FMallocAnsi: out of memory allocating %llu bytes (alignment %u)\n",
			static_cast<unsigned long long>(Count), Alignment);
		FPlatformMisc::RequestExit(true);
		std::abort();
	}

#if PLATFORM_WINDOWS
	// Block layout: [padding][original pointer][requested size][user data...]; the user pointer is aligned.
	struct FAllocationHeader
	{
		void* Original;
		SIZE_T Size;
	};

	FORCEINLINE FAllocationHeader* GetHeader(void* Ptr)
	{
		return reinterpret_cast<FAllocationHeader*>(static_cast<uint8*>(Ptr) - sizeof(FAllocationHeader));
	}

	void* AnsiMalloc(SIZE_T Count, uint32 Alignment)
	{
		void* Original = std::malloc(Count + Alignment + sizeof(FAllocationHeader));
		if (!Original)
		{
			return nullptr;
		}
		void* Result = Align(static_cast<uint8*>(Original) + sizeof(FAllocationHeader), Alignment);
		FAllocationHeader* Header = GetHeader(Result);
		Header->Original = Original;
		Header->Size = Count;
		return Result;
	}

	FORCEINLINE SIZE_T AnsiUsableSize(void* Ptr)
	{
		return GetHeader(Ptr)->Size;
	}

	FORCEINLINE void AnsiFree(void* Ptr)
	{
		std::free(GetHeader(Ptr)->Original);
	}
#else
	void* AnsiMalloc(SIZE_T Count, uint32 Alignment)
	{
		// malloc(0) may return nullptr; allocate one byte so every call yields a unique pointer.
		const SIZE_T Bytes = Count ? Count : 1;
		return Alignment <= NativeAlignment ? std::malloc(Bytes) : memalign(Alignment, Bytes);
	}

	FORCEINLINE SIZE_T AnsiUsableSize(void* Ptr)
	{
		return malloc_usable_size(Ptr);
	}

	FORCEINLINE void AnsiFree(void* Ptr)
	{
		std::free(Ptr);
	}
#endif
} // namespace

void FMallocAnsi::TrackAlloc(SIZE_T Size)
{
	const int64 NewCurrent =
		FPlatformAtomics::InterlockedAdd(&CurrentBytes, static_cast<int64>(Size)) + static_cast<int64>(Size);
	FPlatformAtomics::InterlockedIncrement(&NumAllocations);
	int64 Peak = FPlatformAtomics::AtomicRead(&PeakBytes);
	while (NewCurrent > Peak)
	{
		const int64 Seen = FPlatformAtomics::InterlockedCompareExchange(&PeakBytes, NewCurrent, Peak);
		if (Seen == Peak)
		{
			break;
		}
		Peak = Seen;
	}
}

void FMallocAnsi::TrackFree(SIZE_T Size)
{
	FPlatformAtomics::InterlockedAdd(&CurrentBytes, -static_cast<int64>(Size));
	FPlatformAtomics::InterlockedDecrement(&NumAllocations);
}

void* FMallocAnsi::Malloc(SIZE_T Count, uint32 Alignment)
{
	Alignment = EffectiveAlignment(Alignment);
	void* Result = AnsiMalloc(Count, Alignment);
	if (UNLIKELY(!Result))
	{
		OutOfMemory(Count, Alignment);
	}
	TrackAlloc(AnsiUsableSize(Result));
	return Result;
}

void* FMallocAnsi::Realloc(void* Original, SIZE_T Count, uint32 Alignment)
{
	if (!Original)
	{
		return Malloc(Count, Alignment);
	}
	if (Count == 0)
	{
		Free(Original);
		return nullptr;
	}
	const SIZE_T OldSize = AnsiUsableSize(Original);
#if !PLATFORM_WINDOWS
	if (EffectiveAlignment(Alignment) <= NativeAlignment)
	{
		void* Resized = std::realloc(Original, Count);
		if (UNLIKELY(!Resized))
		{
			OutOfMemory(Count, Alignment);
		}
		TrackFree(OldSize);
		TrackAlloc(AnsiUsableSize(Resized));
		return Resized;
	}
#endif
	// Allocate + copy + free keeps larger alignments (C realloc only guarantees the native one).
	void* Result = Malloc(Count, Alignment);
	std::memcpy(Result, Original, OldSize < Count ? OldSize : Count);
	Free(Original);
	return Result;
}

void FMallocAnsi::Free(void* Original)
{
	if (Original)
	{
		TrackFree(AnsiUsableSize(Original));
		AnsiFree(Original);
	}
}

bool FMallocAnsi::GetAllocationSize(void* Original, SIZE_T& SizeOut)
{
	if (!Original)
	{
		return false;
	}
	SizeOut = AnsiUsableSize(Original);
	return true;
}

FMallocUsage FMallocAnsi::GetUsage() const
{
	FMallocUsage Usage;
	Usage.CurrentBytes = static_cast<uint64>(FPlatformAtomics::AtomicRead(&CurrentBytes));
	Usage.PeakBytes = static_cast<uint64>(FPlatformAtomics::AtomicRead(&PeakBytes));
	Usage.NumAllocations = static_cast<uint64>(FPlatformAtomics::AtomicRead(&NumAllocations));
	return Usage;
}
