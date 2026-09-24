#pragma once

#include "CoreTypes.h"

/**
 * Offline cook entry point (UE: UCookCommandlet, run as `UE4Editor-Cmd -run=cook`):
 * `staticmesh`, `character`, `anim` and `recipe` modes. Returns a process exit code.
 */
class COOKER_API UCookCommandlet
{
public:
	static int32 Main(int32 ArgC, char** ArgV);
};
