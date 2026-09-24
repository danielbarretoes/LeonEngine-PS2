#include "Ps2GsContext.h"

#if defined(LEON_PLATFORM_PS2)
#include <graph.h>

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

int AllocateVram(int width, int height, int psm, int alignment) {
    const int address = graph_vram_allocate(width, height, psm, alignment);
    if (address >= 0) {
        const int end = address + graph_vram_size(width, height, psm, alignment);
        GsContext& gs = GetGsContext();
        if (end > gs.vramEndWords) {
            gs.vramEndWords = end;
        }
    }
    return address;
}

} // namespace leon::rhi::ps2

#endif
