#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

#include <cstdio>
#include <iopcontrol.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <sifrpc.h>

namespace
{
	void (*GFatalExitHandler)(uint8 ReturnCode) = nullptr;
	bool GIopInitialized = false;
} // namespace

void FPS2PlatformMisc::InitializeIop(bool bReset)
{
	if (GIopInitialized)
	{
		return;
	}
	GIopInitialized = true;
	SifInitRpc(0);
	if (bReset)
	{
		while (!SifIopReset("", 0))
		{
		}
		while (!SifIopSync())
		{
		}
		SifInitRpc(0);
	}
	// Older consoles' LOADFILE cannot load a module from EE memory (audsrv.irx) without the first patch.
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
	std::printf("FPS2PlatformMisc: IOP %s, SIF RPC and LOADFILE patches ready\n", bReset ? "reset" : "kept");
}

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
