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
	 * Called once by a forced exit before the EE halts: the launcher shows the error on the TV with it (a player has
	 * no EE console). The handler may not return.
	 */
	static void SetFatalExitHandler(void (*Handler)(uint8 ReturnCode));
};

typedef FPS2PlatformMisc FPlatformMisc;
