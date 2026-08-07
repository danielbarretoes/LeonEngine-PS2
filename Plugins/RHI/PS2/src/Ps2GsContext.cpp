#include "Ps2GsContext.h"

#if defined(LEON_PLATFORM_PS2)

namespace leon::rhi::ps2 {

float GsContext::OriginX() const {
    return 2048.0f - (static_cast<float>(frame.width) * 0.5f);
}

float GsContext::OriginY() const {
    return 2048.0f - (static_cast<float>(frame.height) * 0.5f);
}

GsContext& GetGsContext() {
    static GsContext context{};
    return context;
}

} // namespace leon::rhi::ps2

#endif
