#pragma once

#include "GenericPlatform/GenericPlatformOutputDevices.h"

// Windows adds the debugger channel; the other platforms use the generic console device.
#if PLATFORM_WINDOWS
struct CORE_API FWindowsPlatformOutputDevices : public FGenericPlatformOutputDevices
{
	static void SetupOutputDevices(FOutputDeviceRedirector& Log);
};
typedef FWindowsPlatformOutputDevices FPlatformOutputDevices;
#else
typedef FGenericPlatformOutputDevices FPlatformOutputDevices;
#endif
