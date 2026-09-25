#include "CoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

// Runs the automation tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST) of every module in the closure.
//   -automation=<filter>  run only the tests whose name contains <filter>
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));
	FConfigCacheIni::InitializeConfigSystem();
	FModuleManager::Get().StartupStaticallyLinkedModules();

	FString Filter;
	(void)FParse::Value(FCommandLine::Get(), "automation=", Filter);
	const int32 Failures = FAutomationTestFramework::Get().RunTests(*Filter);

	FModuleManager::Get().ShutdownModules();
	return Failures != 0 ? 1 : 0;
}
