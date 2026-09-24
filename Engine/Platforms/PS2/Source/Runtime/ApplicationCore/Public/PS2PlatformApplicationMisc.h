#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

/** PS2: GS window + DualShock on port 0 (FPS2Application in Private/PS2Application.cpp). */
struct APPLICATIONCORE_API FPS2PlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static GenericApplication* CreateApplication();
};

typedef FPS2PlatformApplicationMisc FPlatformApplicationMisc;
