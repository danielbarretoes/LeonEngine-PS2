#pragma once

// Private GS display state for the PS2 RHI plugin (not a public Engine header).

#include <draw.h>
#include <packet.h>

namespace Leon::PS2 {

struct FPS2GSContext {
    framebuffer_t Frame{};
    zbuffer_t Z{};
    packet_t* Packet = nullptr;
    bool bReady = false;
    /// End of the libgraph bump allocator (32-bit words) — VRAM in use for QueryGpuMemory.
    int VramEndWords = 0;

    [[nodiscard]] float OriginX() const;
    [[nodiscard]] float OriginY() const;
};

[[nodiscard]] FPS2GSContext& GetGSContext();

/// graph_vram_allocate + VRAM usage bookkeeping. Returns word address, or < 0 when full.
[[nodiscard]] int AllocateVram(int Width, int Height, int Psm, int Alignment);

} // namespace Leon::PS2
