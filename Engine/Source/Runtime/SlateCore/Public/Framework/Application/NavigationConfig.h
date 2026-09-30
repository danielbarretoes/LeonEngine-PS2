#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Types/SlateEnums.h"

/**
 * Which keys move the focus and which accept or go back (UE: FNavigationConfig, Slate's): the arrow keys and the
 * gamepad's d-pad move it; Enter, the space bar and the gamepad's bottom face button (the DualShock's Cross) accept;
 * Escape, Backspace and the right face button (Circle) go back. Leon keeps the defaults only.
 */
class SLATECORE_API FNavigationConfig
{
public:
	/** The direction Key moves the focus in, or EUINavigation::Invalid (UE: GetNavigationDirectionFromKey). */
	[[nodiscard]] static EUINavigation GetNavigationDirectionFromKey(const FKey& Key);

	/** What Key does to the focused widget, or EUINavigationAction::Invalid (UE: GetNavigationActionFromKey). */
	[[nodiscard]] static EUINavigationAction GetNavigationActionFromKey(const FKey& Key);
};
