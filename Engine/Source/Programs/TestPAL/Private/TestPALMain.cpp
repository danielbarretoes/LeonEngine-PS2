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
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogTestPAL, Log, All);

// Runs the automation tests linked into this program (Core, CoreUObject, Json, Projects, PakFile and GSCore
// Private/Tests) and prints a verdict line:
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

	// The object system as the registration left it (PS2 budget: Engine/Platforms/PS2/Documentation/Budgets.md).
	const FUObjectReflectionStats Reflection = GetUObjectReflectionStats();
	UE_LOG(LogTestPAL, Display,
		TEXT(
			"Reflection: %d classes, %d structs, %d enums, %d functions, %d properties, %d packages; construction heap "
			"%d KB"),
		Reflection.NumClasses, Reflection.NumStructs, Reflection.NumEnums, Reflection.NumFunctions,
		Reflection.NumProperties, Reflection.NumPackages, int32(Reflection.ConstructionHeapBytes / 1024));
	UE_LOG(LogTestPAL, Display, TEXT("UObject array: %d slots of %d bytes (%d KB), %d objects after registration"),
		GUObjectArray.GetObjectArrayCapacity(), int32(sizeof(FUObjectItem)),
		int32(GUObjectArray.GetAllocatedSize() / 1024), GUObjectArray.GetObjectArrayNumMinusAvailable());

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
	const int32 ObjectsAfterTests = GUObjectArray.GetObjectArrayNumMinusAvailable();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	const FGarbageCollectionStats& GCStats = GetLastGarbageCollectionStats();
	UE_LOG(LogTestPAL, Display,
		TEXT(
			"UObjects: %d objects after the tests, %d after a final garbage collection (%.3f ms, heap %d KB -> %d KB)"),
		ObjectsAfterTests, GCStats.NumObjectsAfter, (GCStats.MarkSeconds + GCStats.PurgeSeconds) * 1000.0,
		int32(GCStats.HeapBytesBefore / 1024), int32(GCStats.HeapBytesAfter / 1024));
	UE_LOG(LogTestPAL, Display, TEXT("TestPAL: %s (%d test(s), %d failed)"), NumFailed ? "FAILED" : "PASSED", NumRun,
		NumFailed);
	GLog->Flush();

	FModuleManager::Get().ShutdownModules();
	return NumFailed ? 1 : 0;
}
