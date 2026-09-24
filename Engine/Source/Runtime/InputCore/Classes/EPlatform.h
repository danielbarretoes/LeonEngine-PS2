#pragma once

#include "EKey.h"

namespace leon {

[[nodiscard]] inline constexpr EPlatform GetCompileTimePlatform() {
#if defined(LEON_PLATFORM_PS2)
    return EPlatform::Ps2;
#else
    return EPlatform::Host;
#endif
}

} // namespace leon
