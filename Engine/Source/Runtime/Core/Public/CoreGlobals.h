#pragma once

#include "CoreTypes.h"

/** True once something asked the engine loop to stop (UE: IsEngineExitRequested). */
CORE_API bool IsEngineExitRequested();

/** Asks the engine loop to stop at the end of the current frame (UE: RequestEngineExit). */
CORE_API void RequestEngineExit(const char* ReasonString);
