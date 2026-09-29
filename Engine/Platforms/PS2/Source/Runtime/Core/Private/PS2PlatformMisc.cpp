#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

#include <cstdio>
#include <cstring>
#include <iopcontrol.h>
#include <kernel.h>
#include <loadfile.h>
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

namespace
{
	int32 GIopLock = -1;
} // namespace

void FPS2PlatformMisc::LockIop()
{
	if (GIopLock < 0)
	{
		// First used on the game thread, before the IO thread exists.
		ee_sema_t Sema{};
		Sema.init_count = 1;
		Sema.max_count = 1;
		GIopLock = CreateSema(&Sema);
	}
	WaitSema(GIopLock);
}

void FPS2PlatformMisc::UnlockIop()
{
	SignalSema(GIopLock);
}

bool FPS2PlatformMisc::LoadIopModule(const char* Path)
{
	// Few modules: a fixed table (no heap this early).
	constexpr int32 MaxModules = 8;
	static const char* Loaded[MaxModules] = {};
	static int32 NumLoaded = 0;
	for (int32 Index = 0; Index < NumLoaded; ++Index)
	{
		if (std::strcmp(Loaded[Index], Path) == 0)
		{
			return true;
		}
	}
	InitializeIop(false);
	LockIop();
	const bool bLoaded = SifLoadModule(Path, 0, nullptr) >= 0;
	UnlockIop();
	if (!bLoaded)
	{
		std::printf("FPS2PlatformMisc: IOP module %s could not be loaded\n", Path);
		return false;
	}
	if (NumLoaded < MaxModules)
	{
		Loaded[NumLoaded++] = Path;
	}
	return true;
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
