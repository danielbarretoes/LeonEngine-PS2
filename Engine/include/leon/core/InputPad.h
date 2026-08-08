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

/// Refresh cached pad state (call once per frame before queries).
void PollPad();

[[nodiscard]] bool IsPadButtonPressed(EPadButton button);

[[nodiscard]] PadStick GetPadLeftStick();
[[nodiscard]] PadStick GetPadRightStick();

} // namespace leon
