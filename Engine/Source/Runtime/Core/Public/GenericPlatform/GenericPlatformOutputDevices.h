#pragma once

#include "CoreTypes.h"

class FOutputDeviceRedirector;

/** Default log devices of a platform (UE: FGenericPlatformOutputDevices). */
struct CORE_API FGenericPlatformOutputDevices
{
	/** Adds the platform's console / debug devices to a new GLog. */
	static void SetupOutputDevices(FOutputDeviceRedirector& Log);
};
