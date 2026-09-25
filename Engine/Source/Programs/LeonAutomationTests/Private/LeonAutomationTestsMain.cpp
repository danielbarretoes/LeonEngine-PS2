#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/CString.h"
#include "Modules/ModuleManager.h"

#include <catch2/catch_session.hpp>

#include <vector>

// Runs the automation tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST) and the Catch2 tests of every module in the closure.
// Leon arguments, removed before Catch2 parses the rest:
//   -automation=<filter>  run only the automation tests whose name contains <filter>
//   -noautomation         skip the automation tests
//   -automationonly       skip the Catch2 tests
int main(int ArgC, char* ArgV[])
{
	FModuleManager::Get().StartupStaticallyLinkedModules();

	const char* AutomationFilter = "";
	bool bRunAutomation = true;
	bool bRunCatch = true;
	std::vector<char*> CatchArgs;
	CatchArgs.push_back(ArgV[0]);
	for (int Index = 1; Index < ArgC; ++Index)
	{
		const char* Arg = ArgV[Index];
		if (FCString::Strnicmp(Arg, "-automation=", 12) == 0)
		{
			AutomationFilter = Arg + 12;
		}
		else if (FCString::Stricmp(Arg, "-noautomation") == 0)
		{
			bRunAutomation = false;
		}
		else if (FCString::Stricmp(Arg, "-automationonly") == 0)
		{
			bRunCatch = false;
		}
		else
		{
			CatchArgs.push_back(ArgV[Index]);
		}
	}

	int32 AutomationFailures = 0;
	if (bRunAutomation)
	{
		AutomationFailures = FAutomationTestFramework::Get().RunTests(AutomationFilter);
	}

	int CatchResult = 0;
	if (bRunCatch)
	{
		CatchResult = Catch::Session().run(static_cast<int>(CatchArgs.size()), CatchArgs.data());
	}

	FModuleManager::Get().ShutdownModules();
	return (CatchResult != 0 || AutomationFailures != 0) ? 1 : 0;
}
