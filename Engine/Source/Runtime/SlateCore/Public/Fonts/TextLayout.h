#pragma once

#include <cstdint>


/// Horizontal justification for HUD / UserWidget text (Unreal-like ETextJustify lite).
enum class ETextJustify : std::uint8_t {
    Left = 0,
    Center = 1,
    Right = 2,
};

/// Default HUD bitmap font scale (stb_easy_font × this).
inline constexpr float kHudFontScale = 2.0f;
/// Line step in pixels at `kHudFontScale` (stb cell height 14).
inline constexpr float kHudLineHeight = 14.0f * kHudFontScale;

