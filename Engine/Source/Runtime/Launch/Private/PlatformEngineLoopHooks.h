#pragma once

#include "CoreTypes.h"

class FGenericWindow;
class GenericApplication;

/**
 * Per-platform hooks around the FEngineLoop frame (WITH_ENGINE=0 path), implemented under
 * Private/<Platform|Group>. Frame order: poll devices -> ticker -> EndFrame -> present -> PostPresent.
 */
struct FPlatformEngineLoopHooks
{
	/** After game work, before present (PS2: engine stats overlay + gamepad widget). */
	static void EndFrame(FGenericWindow& Window, GenericApplication& Application);

	/** After present: start of the next frame's measurements. */
	static void PostPresent();
};
