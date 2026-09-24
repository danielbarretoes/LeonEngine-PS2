#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

/** Linux: GLFW-based application and windows (see Private/Desktop). */
struct APPLICATIONCORE_API FLinuxPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static GenericApplication* CreateApplication();
};

typedef FLinuxPlatformApplicationMisc FPlatformApplicationMisc;
