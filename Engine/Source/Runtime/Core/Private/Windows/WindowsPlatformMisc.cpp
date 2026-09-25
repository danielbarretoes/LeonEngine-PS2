#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "Windows/WindowsHWrapper.h"

#include <combaseapi.h>
#include <cstdio>
#include <cstring>

void FWindowsPlatformMisc::LocalPrint(const TCHAR* Message)
{
	// TCHAR is UTF-8; the debugger's ANSI channel shows ASCII exactly.
	OutputDebugStringA(Message);
}

bool FWindowsPlatformMisc::IsDebuggerPresent()
{
	return ::IsDebuggerPresent() != FALSE;
}

void FWindowsPlatformMisc::RequestExitWithStatus(bool bForce, uint8 ReturnCode)
{
	if (bForce)
	{
		std::fflush(stdout);
		std::fflush(stderr);
		TerminateProcess(GetCurrentProcess(), ReturnCode);
	}
	RequestEngineExit("FPlatformMisc::RequestExit");
}

void FWindowsPlatformMisc::CreateGuid(FGuid& Result)
{
	static_assert(sizeof(GUID) == sizeof(FGuid), "GUID and FGuid must have the same size");
	GUID NewGuid;
	verify(CoCreateGuid(&NewGuid) == S_OK);
	std::memcpy(&Result, &NewGuid, sizeof(Result));
}
