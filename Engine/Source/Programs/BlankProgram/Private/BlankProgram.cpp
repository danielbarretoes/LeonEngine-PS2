#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlankProgram, Log, All);

int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));

	FModuleManager& ModuleManager = FModuleManager::Get();
	ModuleManager.StartupStaticallyLinkedModules();

	FString Modules;
	for (int32 Index = 0; Index < ModuleManager.GetModuleCount(); ++Index)
	{
		Modules += " ";
		Modules += ModuleManager.GetModuleName(Index);
	}
	UE_LOG(LogBlankProgram, Display, "BlankProgram: platform %s, engine %d.%d.%d, %d module(s):%s",
		FPlatformProperties::PlatformName(), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION,
		ModuleManager.GetModuleCount(), *Modules);

	ModuleManager.ShutdownModules();
	return 0;
}
