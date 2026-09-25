#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

#include <Windows.h>
#include <cstdio>

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
