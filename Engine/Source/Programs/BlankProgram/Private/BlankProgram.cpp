#include "CoreTypes.h"
#include "HAL/PlatformProperties.h"
#include "Modules/ModuleManager.h"

#include <cstdio>

int main()
{
	FModuleManager& ModuleManager = FModuleManager::Get();
	ModuleManager.StartupStaticallyLinkedModules();

	std::printf("BlankProgram: platform %s, engine %d.%d.%d, %d module(s):", FPlatformProperties::PlatformName(),
		ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION, ModuleManager.GetModuleCount());
	for (int32 Index = 0; Index < ModuleManager.GetModuleCount(); ++Index)
	{
		std::printf(" %s", ModuleManager.GetModuleName(Index));
	}
	std::printf("\n");

	ModuleManager.ShutdownModules();
	return 0;
}
