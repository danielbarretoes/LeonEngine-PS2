#pragma once

#include "CoreTypes.h"

class GenericApplication;

/** Application-level platform helpers (UE: FGenericPlatformApplicationMisc). */
struct APPLICATIONCORE_API FGenericPlatformApplicationMisc
{
	/** Creates the platform application; the caller owns (deletes) it. */
	static GenericApplication* CreateApplication();
};
