#pragma once

namespace leon {
class Window;
}

namespace leon::ps2lab {

/// Flow: Ps2Lab demo
/// 1. Warm-up frames (clear + triangle)
/// 2. InitializePad + LPS2 header check
/// 3. Mode loop until Start
[[nodiscard]] int RunPs2LabDemo(Window& window);

} // namespace leon::ps2lab
