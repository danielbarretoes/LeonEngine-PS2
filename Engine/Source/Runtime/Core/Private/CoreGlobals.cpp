#include "CoreGlobals.h"

#include <cstdio>

namespace
{
	bool GIsRequestingExit = false;
	uint8 GRequestedExitCode = 0;
} // namespace

uint8 GetRequestedEngineExitCode()
{
	return GRequestedExitCode;
}

void SetRequestedEngineExitCode(uint8 ReturnCode)
{
	GRequestedExitCode = ReturnCode;
}

bool IsEngineExitRequested()
{
	return GIsRequestingExit;
}

void RequestEngineExit(const char* ReasonString)
{
	if (!GIsRequestingExit)
	{
		std::printf("RequestEngineExit: %s\n", ReasonString != nullptr ? ReasonString : "");
	}
	GIsRequestingExit = true;
}
