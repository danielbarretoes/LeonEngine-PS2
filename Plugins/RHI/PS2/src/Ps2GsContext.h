#pragma once

// Private GS display state for the PS2 RHI plugin (not a public Engine header).

#if defined(LEON_PLATFORM_PS2)
#include <draw.h>
#include <packet.h>

namespace leon::rhi::ps2 {

struct GsContext {
    framebuffer_t frame{};
    zbuffer_t z{};
    packet_t* packet = nullptr;
    bool ready = false;

    [[nodiscard]] float OriginX() const;
    [[nodiscard]] float OriginY() const;
};

[[nodiscard]] GsContext& GetGsContext();

} // namespace leon::rhi::ps2
#endif
