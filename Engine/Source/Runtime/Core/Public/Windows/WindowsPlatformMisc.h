#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

struct CORE_API FWindowsPlatformMisc : public FGenericPlatformMisc
{
	static void LocalPrint(const TCHAR* Message);
	static bool IsDebuggerPresent();
	static void RequestExitWithStatus(bool bForce, uint8 ReturnCode);
	static void RequestExit(bool bForce)
	{
		RequestExitWithStatus(bForce, 3);
	}
};

typedef FWindowsPlatformMisc FPlatformMisc;
