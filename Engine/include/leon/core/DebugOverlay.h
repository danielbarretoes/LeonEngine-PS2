#pragma once

namespace leon {

/// Engine debug overlay (PS2), drawn by Window::SwapBuffers for every project:
/// - Stats HUD (top-left): FPS + work ms, RAM, VRAM, RES (+ project extra lines).
/// - Pad widget (top-right): see DrawPadDebugOverlay.
/// Same margin / padding, 50% translucent panels, half-size 5×7 font.
/// Select cycles: both → stats only → pad only → none → both (both at boot).
void SetStatsHudVisible(bool visible);
[[nodiscard]] bool IsStatsHudVisible();

void SetPadDebugOverlayVisible(bool visible);
[[nodiscard]] bool IsPadDebugOverlayVisible();

/// Project debug lines appended under the engine stats (same panel, toggled with it).
/// slot < kStatsHudExtraLines; nullptr or "" clears the slot. Text is copied.
inline constexpr unsigned kStatsHudExtraLines = 4;
void SetStatsHudExtraLine(unsigned slot, const char* text);

/// Window::SwapBuffers hooks: draw before the vsync wait, mark the frame start after it.
void DrawEngineDebugOverlay(int screenWidth, int screenHeight);
void MarkEngineFrameStart();

} // namespace leon
