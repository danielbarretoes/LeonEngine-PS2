#pragma once

#include "CoreMinimal.h"

/**
 * Shows a frame no sooner than SyncInterval fields of 59.94 Hz after the last one, as the PS2's FPS2RHI::WaitVSync
 * does (Docs/PLANS/ps2-preview.md V1): the desktop's frame rate and delta times are the console's. A late frame goes at
 * once and the next ones count from it; -benchmark (FApp::IsBenchmarking) and an interval of 0 never wait.
 */
class FFramePacer
{
public:
	static constexpr double FieldSeconds = 1.0 / 59.94;

	/** Waits until the frame may be shown (the clock is FPlatformTime::Seconds). */
	void Wait(int32 SyncInterval);

	/** The time Wait released the last frame at. */
	[[nodiscard]] double GetLastFrameSeconds() const
	{
		return LastFrameSeconds;
	}

	/** When the frame after one shown at LastFrame may be shown (a late frame resets the count). */
	[[nodiscard]] static double NextFrameSeconds(double LastFrame, double Now, int32 SyncInterval);

private:
	double LastFrameSeconds = 0.0;
};
