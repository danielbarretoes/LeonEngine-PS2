#pragma once

#include "CoreTypes.h"

/**
 * Engine debug overlay state (UE: stat unit/fps + GEngine->AddOnScreenDebugMessage, reduced). The stats' visibility is
 * the engine's `stat unit` panel's (UEngine::SetHudStatsVisible, F4 on the desktop) and the platform's own panel's
 * (PS2: Launch's PS2StatsOverlay, the EE's work and the gamepad widget, cycled with L3 + R3: both -> stats only ->
 * gamepad only -> none -> both).
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

	/** Advances the cycle (both -> stats -> gamepad -> none). */
	static void CycleVisibility();

	/** Sets the text of an on-screen debug message slot (copied); nullptr or "" clears it. */
	static void AddOnScreenDebugMessage(int32 Key, const char* Message);
	static void ClearOnScreenDebugMessage(int32 Key);

	/** The message in a slot, or "" when empty. */
	static const char* GetOnScreenDebugMessage(int32 Key);
};
