#pragma once

#include "CoreTypes.h"

/** True once something asked the engine loop to stop (UE: IsEngineExitRequested). */
CORE_API bool IsEngineExitRequested();

/** Asks the engine loop to stop at the end of the current frame (UE: RequestEngineExit). */
CORE_API void RequestEngineExit(const char* ReasonString);

class FOutputDeviceRedirector;

/** The global log, created on first use with the platform's default devices (UE: GLog). */
CORE_API FOutputDeviceRedirector* GetGlobalLogSingleton();
#define GLog GetGlobalLogSingleton()
