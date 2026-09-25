#include "HAL/PlatformMisc.h"
#include "HAL/UnrealMemory.h"

#include <new>

// C++ runtime hooks for the EE (UE: REPLACEMENT_OPERATOR_NEW_AND_DELETE in monolithic builds).
//
// libstdc++'s operator new throws std::bad_alloc and its __cxa_pure_virtual calls std::terminate; either one links the
// exception unwinder and the verbose terminate handler with its C++ demangler (about 85 KB of text) into a program
// built with -fno-exceptions. These replacements route new / delete through FMemory, so GMalloc sees every C++
// allocation, and turn a pure virtual call into a fatal error.

extern "C" void __cxa_pure_virtual()
{
	FPlatformMisc::LowLevelOutputDebugString("Fatal error: pure virtual function called\n");
	FPlatformMisc::RequestExitWithStatus(true, 3);
	for (;;)
	{
	}
}

extern "C" void __cxa_deleted_virtual()
{
	FPlatformMisc::LowLevelOutputDebugString("Fatal error: deleted virtual function called\n");
	FPlatformMisc::RequestExitWithStatus(true, 3);
	for (;;)
	{
	}
}

namespace
{
	void* AllocateOrDie(std::size_t Size)
	{
		void* Result = FMemory::Malloc(Size ? SIZE_T(Size) : 1);
		if (Result == nullptr)
		{
			FPlatformMisc::LowLevelOutputDebugString("Fatal error: out of memory in operator new\n");
			FPlatformMisc::RequestExitWithStatus(true, 3);
		}
		return Result;
	}
} // namespace

void* operator new(std::size_t Size)
{
	return AllocateOrDie(Size);
}

void* operator new[](std::size_t Size)
{
	return AllocateOrDie(Size);
}

void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
	return FMemory::Malloc(Size ? SIZE_T(Size) : 1);
}

void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept
{
	return FMemory::Malloc(Size ? SIZE_T(Size) : 1);
}

void operator delete(void* Ptr) noexcept
{
	FMemory::Free(Ptr);
}

void operator delete[](void* Ptr) noexcept
{
	FMemory::Free(Ptr);
}

void operator delete(void* Ptr, std::size_t) noexcept
{
	FMemory::Free(Ptr);
}

void operator delete[](void* Ptr, std::size_t) noexcept
{
	FMemory::Free(Ptr);
}

void operator delete(void* Ptr, const std::nothrow_t&) noexcept
{
	FMemory::Free(Ptr);
}

void operator delete[](void* Ptr, const std::nothrow_t&) noexcept
{
	FMemory::Free(Ptr);
}
