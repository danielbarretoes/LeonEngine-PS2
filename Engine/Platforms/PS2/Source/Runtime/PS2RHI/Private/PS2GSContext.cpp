#include "PS2GSContext.h"

#include <graph.h>

namespace Leon::PS2 {

float FPS2GSContext::OriginX() const {
    return 2048.0f - (static_cast<float>(frame.width) * 0.5f);
}

float FPS2GSContext::OriginY() const {
    return 2048.0f - (static_cast<float>(frame.height) * 0.5f);
}

FPS2GSContext& GetGSContext() {
    static FPS2GSContext context{};
    return context;
}

int AllocateVram(int width, int height, int psm, int alignment) {
    const int address = graph_vram_allocate(width, height, psm, alignment);
    if (address >= 0) {
        const int end = address + graph_vram_size(width, height, psm, alignment);
        FPS2GSContext& gs = GetGSContext();
        if (end > gs.vramEndWords) {
            gs.vramEndWords = end;
        }
    }
    return address;
}

} // namespace Leon::PS2

