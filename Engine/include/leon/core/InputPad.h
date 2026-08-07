#pragma once

#include <leon/core/EKey.h>

namespace leon {

[[nodiscard]] bool InitializePs2Pad();
[[nodiscard]] bool IsPadButtonPressed(EPadButton button);

} // namespace leon
