#include "CoreGlobals.h"

#include <cstdio>

namespace
{
	bool GIsRequestingExit = false;
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
