#pragma once

#include "CoreTypes.h"

/** Horizontal justification of text (UE: ETextJustify). */
enum class ETextJustify : uint8
{
	Left = 0,
	Center = 1,
	Right = 2,
};

/**
 * Default HUD bitmap font scale (stb_easy_font x this): 1, the font's own pixels, on the GS's 640 x 448 frame that
 * every platform draws the canvas into (Docs/PLANS/ps2-preview.md V1). A fractional scale gives uneven strokes (the
 * glyphs are rectangles snapped to whole pixels).
 */
inline constexpr float HudFontScale = 1.0f;
/** Line step in pixels at HudFontScale (stb cell height 14). */
inline constexpr float HudLineHeight = 14.0f * HudFontScale;
