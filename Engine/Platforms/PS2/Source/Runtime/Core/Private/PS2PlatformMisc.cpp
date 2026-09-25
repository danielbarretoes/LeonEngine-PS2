#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

#include <cstdio>
#include <kernel.h>

void FPS2PlatformMisc::RequestExitWithStatus(bool bForce, uint8 ReturnCode)
{
	if (!bForce)
	{
		RequestEngineExit("FPlatformMisc::RequestExit");
		return;
	}
	// Returning to the browser would lose the EE console context; halt instead.
	std::printf("FPlatformMisc: fatal exit (code %u), EE halted\n", static_cast<unsigned>(ReturnCode));
	std::fflush(stdout);
	for (;;)
	{
		SleepThread();
	}
}
