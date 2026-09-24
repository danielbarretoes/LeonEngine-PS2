#include "CoreTypes.h"
#include "Modules/ModuleManager.h"

#include <catch2/catch_session.hpp>

int main(int ArgC, char* ArgV[])
{
	FModuleManager::Get().StartupStaticallyLinkedModules();
	const int Result = Catch::Session().run(ArgC, ArgV);
	FModuleManager::Get().ShutdownModules();
	return Result;
}
