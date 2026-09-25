#pragma once

#include "CoreTypes.h"

/** Horizontal justification for HUD / UUserWidget text (UE-like ETextJustify lite). */
enum class ETextJustify : uint8
{
	Left = 0,
	Center = 1,
	Right = 2,
};

/** Default HUD bitmap font scale (stb_easy_font x this). */
inline constexpr float HudFontScale = 2.0f;
/** Line step in pixels at HudFontScale (stb cell height 14). */
inline constexpr float HudLineHeight = 14.0f * HudFontScale;
