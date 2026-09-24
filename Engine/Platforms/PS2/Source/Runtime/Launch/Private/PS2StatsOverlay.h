#pragma once

#include "CoreTypes.h"

class IInputInterface;

/**
 * Draws the engine debug overlay (FStatsOverlay state) with the GS at the end of each frame:
 * stats panel top-left (FPS + work ms, RAM, VRAM, RES, on-screen debug messages) and the
 * DualShock widget top-right. Gamepad Special Left (Select) cycles the visibility.
 */
struct FPS2StatsOverlay
{
	/** Game work of the frame is measured from here (after vsync) to Draw. */
	static void MarkFrameStart();

	static void Draw(int32 ScreenWidth, int32 ScreenHeight, IInputInterface* InputInterface);
};
