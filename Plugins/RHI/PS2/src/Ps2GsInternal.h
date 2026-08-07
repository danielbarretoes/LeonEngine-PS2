#pragma once

// Shared GS state for PS2 RHI sources (not a public Engine header).

#if defined(LEON_PLATFORM_PS2)
#include <draw.h>
#include <packet.h>

namespace leon::rhi::ps2gs {

inline framebuffer_t g_frame{};
inline zbuffer_t g_z{};
inline packet_t* g_packet = nullptr;
inline bool g_displayReady = false;

[[nodiscard]] inline float OriginX() {
    return 2048.0f - (static_cast<float>(g_frame.width) * 0.5f);
}

[[nodiscard]] inline float OriginY() {
    return 2048.0f - (static_cast<float>(g_frame.height) * 0.5f);
}

} // namespace leon::rhi::ps2gs
#endif
