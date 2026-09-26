#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

#include <cstdio>
#include <kernel.h>

namespace
{
	void (*GFatalExitHandler)(uint8 ReturnCode) = nullptr;
} // namespace

void FPS2PlatformMisc::SetFatalExitHandler(void (*Handler)(uint8 ReturnCode))
{
	GFatalExitHandler = Handler;
}

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
	if (void (*Handler)(uint8) = GFatalExitHandler)
	{
		// Once: a failure inside the handler halts like one without it.
		GFatalExitHandler = nullptr;
		Handler(ReturnCode);
	}
	for (;;)
	{
		SleepThread();
	}
}
