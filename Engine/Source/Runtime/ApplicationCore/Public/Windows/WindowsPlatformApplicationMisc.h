#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

/** Windows: GLFW-based application and windows (see Private/Desktop). */
struct APPLICATIONCORE_API FWindowsPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static GenericApplication* CreateApplication();
};

typedef FWindowsPlatformApplicationMisc FPlatformApplicationMisc;
