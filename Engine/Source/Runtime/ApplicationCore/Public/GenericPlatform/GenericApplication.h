#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/IInputInterface.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"

APPLICATIONCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogApplicationCore, Log, All);

/**
 * Platform application: creates windows and owns input devices (UE: GenericApplication).
 * Obtain one with FPlatformApplicationMisc::CreateApplication().
 */
class APPLICATIONCORE_API GenericApplication
{
public:
	virtual ~GenericApplication() = default;

	/** Creates an (uninitialised) platform window; call Create() on it. */
	virtual TSharedRef<FGenericWindow> MakeWindow() = 0;

	/** Polls game controllers once per frame (UE: PollGameDeviceState). */
	virtual void PollGameDeviceState()
	{
	}

	/** Game controllers, or nullptr when the platform has none wired. */
	virtual IInputInterface* GetInputInterface()
	{
		return nullptr;
	}
};
