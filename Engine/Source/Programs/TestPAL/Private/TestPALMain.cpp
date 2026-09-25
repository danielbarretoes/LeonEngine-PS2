#include "CoreMinimal.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTestPAL, Log, All);

// Runs the automation tests linked into this program (Core's Private/Tests) and prints a verdict line:
//   TestPAL: PASSED (N test(s), 0 failed)
// Arguments: -filter=<text> runs only the tests whose name contains <text>.
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));
	FConfigCacheIni::InitializeConfigSystem();
	FModuleManager::Get().StartupStaticallyLinkedModules();

	FString Filter;
	FParse::Value(FCommandLine::Get(), "filter=", Filter);

	UE_LOG(LogTestPAL, Display, TEXT("TestPAL on %s, engine %d.%d.%d"), FPlatformProperties::PlatformName(),
		ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION);
	UE_LOG(LogTestPAL, Display, TEXT("Base %s, engine %s, project %s"), FPlatformProcess::BaseDir(),
		*FPaths::EngineDir(), *FPaths::ProjectDir());

	int32 NumRun = 0;
	const int32 NumFailed = FAutomationTestFramework::Get().RunTests(
		*Filter, EAutomationTestFlags::Disabled | EAutomationTestFlags::NonNullRHI, &NumRun);

	// Numbers for the platform budgets (PS2: Engine/Platforms/PS2/Documentation/Budgets.md).
	const FMallocUsage Usage = FMemory::GetUsage();
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	UE_LOG(LogTestPAL, Display,
		TEXT("Memory: GMalloc peak %llu KB, current %llu KB, %llu live allocations; process %llu KB"),
		(unsigned long long)(Usage.PeakBytes / 1024), (unsigned long long)(Usage.CurrentBytes / 1024),
		(unsigned long long)Usage.NumAllocations, (unsigned long long)(Stats.UsedPhysical / 1024));
	UE_LOG(LogTestPAL, Display, TEXT("Names: %d entries, %d KB used of %d KB (blocks + hash)"), FName::GetNumNames(),
		FName::GetNameEntryMemorySize() / 1024, FName::GetNameTableMemorySize() / 1024);
	UE_LOG(LogTestPAL, Display, TEXT("TestPAL: %s (%d test(s), %d failed)"), NumFailed ? "FAILED" : "PASSED", NumRun,
		NumFailed);
	GLog->Flush();

	FModuleManager::Get().ShutdownModules();
	return NumFailed ? 1 : 0;
}
