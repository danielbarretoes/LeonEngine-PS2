#pragma once

#include "CoreTypes.h"

/**
 * Engine debug overlay state (UE: stat unit/fps + GEngine->AddOnScreenDebugMessage, reduced).
 * The platform draws it at the end of each frame (PS2: Launch's PS2StatsOverlay):
 *   - stats panel (top-left): FPS + work ms, RAM, VRAM, RES, then the on-screen debug messages;
 *   - gamepad widget (top-right).
 * Gamepad Special Left (Select) cycles: both -> stats only -> gamepad only -> none -> both.
 */
class CORE_API FStatsOverlay
{
public:
	/** Number of on-screen debug message slots below the engine stats. */
	static constexpr int32 MaxOnScreenMessages = 4;

	static void SetStatsVisible(bool bVisible);
	static bool IsStatsVisible();

	static void SetGamepadWidgetVisible(bool bVisible);
	static bool IsGamepadWidgetVisible();

	/** Advances the Select cycle (both -> stats -> gamepad -> none). */
	static void CycleVisibility();

	/** Sets the text of an on-screen debug message slot (copied); nullptr or "" clears it. */
	static void AddOnScreenDebugMessage(int32 Key, const char* Message);
	static void ClearOnScreenDebugMessage(int32 Key);

	/** The message in a slot, or "" when empty. */
	static const char* GetOnScreenDebugMessage(int32 Key);
};
