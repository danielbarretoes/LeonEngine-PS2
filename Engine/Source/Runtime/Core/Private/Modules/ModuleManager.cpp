#include "Modules/ModuleManager.h"

#include <cstring>

FModuleManager& FModuleManager::Get()
{
	static FModuleManager Instance;
	return Instance;
}

void FModuleManager::StartupStaticallyLinkedModules()
{
	if (Modules != nullptr)
	{
		return;
	}

	int32 Count = 0;
	const FStaticallyLinkedModuleInfo* Infos = GetStaticallyLinkedModules(Count);
	NumModules = Count;
	Modules = new FModuleEntry[Count > 0 ? Count : 1];
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Modules[Index].Name = Infos[Index].Name;
		Modules[Index].Module = Infos[Index].InitializeModule();
		// Record the module's reflected types, then let CoreUObject (once it is up) construct them, before the module
		// starts (UE: the per-module ProcessNewlyLoadedUObjects).
		if (Infos[Index].RegisterReflection)
		{
			Infos[Index].RegisterReflection();
		}
		if (ProcessLoadedObjectsCallback)
		{
			ProcessLoadedObjectsCallback(Infos[Index].Name, true);
		}
		Modules[Index].Module->StartupModule();
	}
}

void FModuleManager::ShutdownModules()
{
	for (int32 Index = NumModules - 1; Index >= 0; --Index)
	{
		if (Modules[Index].Module != nullptr)
		{
			Modules[Index].Module->ShutdownModule();
			delete Modules[Index].Module;
			Modules[Index].Module = nullptr;
		}
	}
	delete[] Modules;
	Modules = nullptr;
	NumModules = 0;
}

IModuleInterface* FModuleManager::GetModule(const char* ModuleName) const
{
	for (int32 Index = 0; Index < NumModules; ++Index)
	{
		if (std::strcmp(Modules[Index].Name, ModuleName) == 0)
		{
			return Modules[Index].Module;
		}
	}
	return nullptr;
}

bool FModuleManager::IsModuleLoaded(const char* ModuleName) const
{
	return GetModule(ModuleName) != nullptr;
}

int32 FModuleManager::GetModuleCount() const
{
	return NumModules;
}

const char* FModuleManager::GetModuleName(int32 Index) const
{
	return (Index >= 0 && Index < NumModules) ? Modules[Index].Name : nullptr;
}
