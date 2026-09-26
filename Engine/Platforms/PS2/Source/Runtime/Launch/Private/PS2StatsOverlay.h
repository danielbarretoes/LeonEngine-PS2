#pragma once

#include "CoreTypes.h"

class IInputInterface;

/**
 * Draws the PS2's part of the debug overlay (FStatsOverlay state) with the GS at the end of each frame, at the top
 * left: the EE's work per frame and the on-screen debug messages, then the DualShock widget below them. The engine's
 * `stat unit` panel (FPS, MS, RAM, VRAM, TRIS, OBJ) is the canvas's, at the top right, and shares the stats'
 * visibility. L3 + R3 together cycle it: both -> stats -> gamepad -> none.
 */
struct FPS2StatsOverlay
{
	/** Game work of the frame is measured from here (after vsync) to Draw. */
	static void MarkFrameStart();

	static void Draw(int32 ScreenWidth, int32 ScreenHeight, IInputInterface* InputInterface);
};
