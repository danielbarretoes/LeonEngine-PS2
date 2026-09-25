#include "CoreMinimal.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformProperties.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformTypesTest, "System.Core.HAL.Types",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPlatformTypesTest::RunTest(const FString& Parameters)
{
	static_assert(sizeof(int8) == 1 && sizeof(int16) == 2 && sizeof(int32) == 4 && sizeof(int64) == 8, "sizes");
	static_assert(sizeof(UPTRINT) == sizeof(void*), "pointer-sized integer");
	static_assert(sizeof(TCHAR) == 1, "TCHAR is UTF-8");
	TestEqual("Exactly one platform macro", PLATFORM_WINDOWS + PLATFORM_LINUX + PLATFORM_PS2, 1);
	TestEqual("Build configuration", UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_SHIPPING, 1);
	TestTrue("Platform name", FCString::Strlen(FPlatformProperties::PlatformName()) > 0);
	TestEqual("INDEX_NONE", int32(INDEX_NONE), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformMathTest, "System.Core.HAL.PlatformMath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPlatformMathTest::RunTest(const FString& Parameters)
{
	TestEqual("CountLeadingZeros(1)", FMath::CountLeadingZeros(1u), 31u);
	TestEqual("CountLeadingZeros(0)", FMath::CountLeadingZeros(0u), 32u);
	TestEqual("CountTrailingZeros(8)", FMath::CountTrailingZeros(8u), 3u);
	TestEqual("CountLeadingZeros64", FMath::CountLeadingZeros64(1ull << 40), uint64(23));
	TestEqual("FloorLog2(1000)", FMath::FloorLog2(1000u), 9u);
	TestEqual("CeilLogTwo(1000)", FMath::CeilLogTwo(1000u), 10u);
	TestEqual("RoundUpToPowerOfTwo(17)", FMath::RoundUpToPowerOfTwo(17u), 32u);
	TestEqual("CountBits", FMath::CountBits(0xF0F0ull), 8);
	TestEqual("Clamp", FMath::Clamp(12, 0, 10), 10);
	TestTrue("IsPowerOfTwo", FMath::IsPowerOfTwo(64u) && !FMath::IsPowerOfTwo(65u));
	TestEqual("DivideAndRoundUp", FMath::DivideAndRoundUp(10, 4), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformAtomicsTest, "System.Core.HAL.Atomics",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPlatformAtomicsTest::RunTest(const FString& Parameters)
{
	volatile int32 Value32 = 5;
	TestEqual("InterlockedIncrement returns the old value", FPlatformAtomics::InterlockedIncrement(&Value32), 5);
	TestEqual("InterlockedAdd", FPlatformAtomics::InterlockedAdd(&Value32, int32(10)), 6);
	TestEqual("InterlockedCompareExchange (hit)",
		FPlatformAtomics::InterlockedCompareExchange(&Value32, int32(1), int32(16)), 16);
	TestEqual("Value after exchange", FPlatformAtomics::AtomicRead(&Value32), 1);
	volatile int64 Value64 = 1ll << 40;
	FPlatformAtomics::InterlockedAdd(&Value64, int64(1));
	TestEqual("64-bit add", int64(FPlatformAtomics::AtomicRead(&Value64)), (1ll << 40) + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModuleManagerTest, "System.Core.Modules.StaticallyLinked",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FModuleManagerTest::RunTest(const FString& Parameters)
{
	FModuleManager& ModuleManager = FModuleManager::Get();
	TestTrue("Core is loaded", ModuleManager.IsModuleLoaded("Core"));
	if (!TestTrue("At least one module", ModuleManager.GetModuleCount() >= 1))
	{
		return false;
	}
	TestEqual("Core starts first", ModuleManager.GetModuleName(0), TEXT("Core"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemoryTest, "System.Core.HAL.Memory",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMemoryTest::RunTest(const FString& Parameters)
{
	const FMallocUsage Before = FMemory::GetUsage();

	void* Block = FMemory::Malloc(100);
	TestNotNull("Malloc", Block);
	TestTrue("Default alignment is 16", IsAligned(Block, 16));
	TestTrue("Usable size covers the request", FMemory::GetAllocSize(Block) >= 100);
	TestTrue("Usage grows", FMemory::GetUsage().CurrentBytes >= Before.CurrentBytes + 100);

	FMemory::Memset(Block, 0xAB, 100);
	Block = FMemory::Realloc(Block, 1000);
	TestEqual("Realloc keeps the contents", int32(static_cast<uint8*>(Block)[99]), 0xAB);

	void* Aligned = FMemory::Malloc(64, 128);
	TestTrue("Explicit alignment", IsAligned(Aligned, 128));
	Aligned = FMemory::Realloc(Aligned, 4096, 128);
	TestTrue("Realloc keeps the alignment", IsAligned(Aligned, 128));

	FMemory::Free(Block);
	FMemory::Free(Aligned);
	TestEqual("Usage returns", int64(FMemory::GetUsage().CurrentBytes), int64(Before.CurrentBytes));
	TestTrue("Peak recorded", FMemory::GetUsage().PeakBytes >= Before.CurrentBytes + 1000);

	void* Zeroed = FMemory::MallocZeroed(32);
	TestEqual("MallocZeroed", int32(static_cast<uint8*>(Zeroed)[31]), 0);
	FMemory::Free(Zeroed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnsureTest, "System.Core.Misc.Ensure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FEnsureTest::RunTest(const FString& Parameters)
{
	#if DO_ENSURE
	// A failed ensure logs an error; the test expects exactly one.
	AddExpectedError(TEXT("Ensure condition failed"), 1);
	const int32 FailuresBefore = FDebug::GetNumEnsureFailures();
	bool bRanBody = false;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		if (!ensureMsgf(Pass < 0, TEXT("reported once per call site (pass %d)"), Pass))
		{
			bRanBody = true;
		}
	}
	TestTrue("ensure returns false", bRanBody);
	TestEqual("Reported once", FDebug::GetNumEnsureFailures() - FailuresBefore, 1);
	#endif
	TestTrue("ensure passes through true", ensure(1 + 1 == 2));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
