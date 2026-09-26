#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

/** The EE console (printf over the SIF / PCSX2 EE log) is the debug channel. */
struct CORE_API FPS2PlatformMisc : public FGenericPlatformMisc
{
	/** A forced exit halts the EE thread so the console keeps the last messages. */
	static void RequestExitWithStatus(bool bForce, uint8 ReturnCode);
	static void RequestExit(bool bForce)
	{
		RequestExitWithStatus(bForce, 3);
	}

	/**
	 * Readies the IOP for the engine's modules, once (the ps2sdk samples' reset_IOP): with bReset, a reboot to the
	 * ROM's modules first, so a launcher's own (uLaunchELF's XSIO2MAN, XPADMAN) do not clash with the ones the engine
	 * loads; then the SIF RPC and the LOADFILE patches that loading a module from EE memory needs. Call it before any
	 * file is opened: a reboot closes the IOP's files. bReset false keeps a debugger's host: (ps2link: -NoIopReset).
	 */
	static void InitializeIop(bool bReset);

	/**
	 * Called once by a forced exit before the EE halts: the launcher shows the error on the TV with it (a player has
	 * no EE console). The handler may not return.
	 */
	static void SetFatalExitHandler(void (*Handler)(uint8 ReturnCode));
};

typedef FPS2PlatformMisc FPlatformMisc;
