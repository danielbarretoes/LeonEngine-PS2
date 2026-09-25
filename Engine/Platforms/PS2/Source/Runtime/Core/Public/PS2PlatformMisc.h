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
};

typedef FPS2PlatformMisc FPlatformMisc;
