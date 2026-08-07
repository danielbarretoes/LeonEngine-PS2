#pragma once

namespace leon {
class Window;
}

namespace leon::ps2cube {

/// Flow: Ps2Cube 3D demo
/// 1. Warm-up clear frames
/// 2. InitializePad
/// 3. Spin / pad-orbit a perspective unlit box until Start
[[nodiscard]] int RunPs2CubeDemo(Window& window);

} // namespace leon::ps2cube
