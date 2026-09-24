#pragma once

#include <leon/core/EKey.h>

namespace leon {

/// Stick axes in [-1, 1]. Y: -1 = up / forward (screen-up).
struct PadStick {
    float X = 0.0f;
    float Y = 0.0f;
};

/// Load IOP pad modules and open port 0 (PS2). Host stub returns false.
[[nodiscard]] bool InitializePad();

/// Refresh cached pad state. Window::PollEvents calls this every frame on PS2; call it
/// yourself only when running without a Window.
void PollPad();

[[nodiscard]] bool IsPadButtonPressed(EPadButton button);

[[nodiscard]] PadStick GetPadLeftStick();
[[nodiscard]] PadStick GetPadRightStick();

/// Debug widget: 50% translucent mini DualShock at screen (x, y) = top-left.
/// Size kPadDebugWidth × kPadDebugHeight units × scale, kPadDebugPadding units inset.
/// LED: red = port not open, orange = port open but no stable read, green = reading.
/// Buttons light up while held; stick dots follow the raw axes. PS2 only (host no-op).
/// The engine already draws it every frame (see leon/core/DebugOverlay.h).
void DrawPadDebugOverlay(float x, float y, float scale = 0.75f);

inline constexpr float kPadDebugWidth = 150.0f;
inline constexpr float kPadDebugHeight = 78.0f;
inline constexpr float kPadDebugPadding = 4.0f;

} // namespace leon
