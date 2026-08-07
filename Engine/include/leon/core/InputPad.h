#pragma once

#include <leon/core/EKey.h>

namespace leon {

/// Load IOP pad modules and open port 0 (PS2). Host stub returns false.
[[nodiscard]] bool InitializePad();

[[nodiscard]] bool IsPadButtonPressed(EPadButton button);

} // namespace leon
