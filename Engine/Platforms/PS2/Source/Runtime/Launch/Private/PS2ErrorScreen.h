#pragma once

#include "CoreTypes.h"

/**
 * The error screen of the PS2 game (UE: a console's fatal error message): the last errors of the log, drawn on the TV
 * with the GS's debug text when the game stops before it plays or on a fatal error. A player has no EE console, and a
 * black screen says nothing.
 */
class FPS2ErrorScreen
{
public:
	/** Starts keeping the log's error lines and shows the screen on a forced exit (FPS2PlatformMisc). */
	static void Install();

	/** Shows the screen for a game that returned ExitCode; does not return. */
	[[noreturn]] static void Show(int32 ExitCode);
};
