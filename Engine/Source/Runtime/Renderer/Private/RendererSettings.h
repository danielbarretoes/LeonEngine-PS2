#pragma once

#include "CoreMinimal.h"

/**
 * The renderer's settings from `[/Script/Engine.RendererSettings]` of the engine config (UE: URendererSettings), read
 * once when the renderer starts. Their defaults are the PS2's (Docs/PLANS/ps2-preview.md D1), on every platform.
 */
struct FRendererSettings
{
	/**
	 * The aspect ratio the frame is shown at (4:3: the TV the PS2's 640 x 448 frame goes to; 16:9 for an anamorphic
	 * game), which the view's projection uses and the desktop presents the frame with. 0: the frame's own.
	 */
	float DisplayAspectRatio = 4.0f / 3.0f;

	/** The vertical blanks (59.94 Hz fields) a frame is shown for at least (UE: rhi.SyncInterval): 2 is 30 fps. */
	int32 SyncInterval = 2;

	/** The engine config's values (the defaults above where it has none). */
	[[nodiscard]] static FRendererSettings Load();

	/** The aspect ratio a TargetSize frame is shown at: DisplayAspectRatio, or the frame's without one. */
	[[nodiscard]] float GetDisplayAspectRatio(const FIntPoint& TargetSize) const;
};
