#include "PS2GSContext.h"
#include "PS2SceneState.h"

#include "PS2RHI.h"

#include <cstring>

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <malloc.h>
#include <packet.h>

namespace {


[[nodiscard]] int NextPow2(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

void FillChecker(unsigned char* rgba, int size) {
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cell = ((x >> 3) ^ (y >> 3)) & 1;
            const unsigned char c = cell ? static_cast<unsigned char>(220) : static_cast<unsigned char>(40);
            const int i = (y * size + x) * 4;
            rgba[i + 0] = c;
            rgba[i + 1] = c;
            rgba[i + 2] = cell ? static_cast<unsigned char>(200) : static_cast<unsigned char>(50);
            rgba[i + 3] = 255;
        }
    }
}

void FillGrid(unsigned char* rgba, int size) {
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool line = (x & 7) == 0 || (y & 7) == 0;
            const int i = (y * size + x) * 4;
            rgba[i + 0] = line ? static_cast<unsigned char>(70) : static_cast<unsigned char>(160);
            rgba[i + 1] = line ? static_cast<unsigned char>(90) : static_cast<unsigned char>(170);
            rgba[i + 2] = line ? static_cast<unsigned char>(80) : static_cast<unsigned char>(150);
            rgba[i + 3] = 255;
        }
    }
}

[[nodiscard]] bool UploadRgba(int vramAddress, int bufferWidth, int width, int height,
                              void* rgbaAligned) {
    packet_t* packet = packet_init(80, PACKET_NORMAL);
    if (packet == nullptr) {
        return false;
    }
    qword_t* q = packet->data;
    q = draw_texture_transfer(q, rgbaAligned, width, height, GS_PSM_32, vramAddress, bufferWidth);
    q = draw_texture_flush(q);
    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();
    packet_free(packet);
    return true;
}

packet_t*& BindPacketSlot() {
    static packet_t* packet = nullptr;
    return packet;
}


} // namespace

FPS2Texture FPS2Texture::CreateFromAlignedRgba(int width, int height, unsigned char* rgba) {
    if (!Leon::PS2::GetGSContext().ready || width <= 0 || height <= 0 || rgba == nullptr) {
        return {};
    }
    const int bufW = NextPow2(width);
    const int addr = Leon::PS2::AllocateVram(bufW, height, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    if (addr < 0) {
        return {};
    }
    if (!UploadRgba(addr, bufW, width, height, rgba)) {
        return {};
    }
    return FPS2Texture(width, height, addr, bufW);
}

FPS2Texture::~FPS2Texture() {
    Destroy();
}

FPS2Texture::FPS2Texture(FPS2Texture&& other) noexcept
    : width_(other.width_), height_(other.height_), vramAddress_(other.vramAddress_),
      bufferWidth_(other.bufferWidth_) {
    other.width_ = 0;
    other.height_ = 0;
    other.vramAddress_ = 0;
    other.bufferWidth_ = 0;
}

FPS2Texture& FPS2Texture::operator=(FPS2Texture&& other) noexcept {
    if (this != &other) {
        Destroy();
        width_ = other.width_;
        height_ = other.height_;
        vramAddress_ = other.vramAddress_;
        bufferWidth_ = other.bufferWidth_;
        other.width_ = 0;
        other.height_ = 0;
        other.vramAddress_ = 0;
        other.bufferWidth_ = 0;
    }
    return *this;
}

void FPS2Texture::Destroy() {
    if (vramAddress_ != 0) {
        Leon::PS2::InvalidateBoundTexture();
    }
    width_ = 0;
    height_ = 0;
    vramAddress_ = 0;
    bufferWidth_ = 0;
}

bool FPS2Texture::Valid() const {
    return width_ > 0 && height_ > 0 && vramAddress_ > 0;
}

FPS2Texture FPS2Texture::Create(int width, int height, const unsigned char* rgba) {
    if (rgba == nullptr || width <= 0 || height <= 0) {
        return {};
    }
    auto* aligned = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(width * height * 4)));
    if (aligned == nullptr) {
        return {};
    }
    std::memcpy(aligned, rgba, static_cast<size_t>(width * height * 4));
    FPS2Texture tex = CreateFromAlignedRgba(width, height, aligned);
    free(aligned);
    return tex;
}

FPS2Texture FPS2Texture::CreateChecker(int size) {
    if (size < 8) {
        size = 8;
    }
    auto* rgba = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(size * size * 4)));
    if (rgba == nullptr) {
        return {};
    }
    FillChecker(rgba, size);
    FPS2Texture tex = CreateFromAlignedRgba(size, size, rgba);
    free(rgba);
    return tex;
}

FPS2Texture FPS2Texture::CreateGrid(int size) {
    if (size < 8) {
        size = 8;
    }
    auto* rgba = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(size * size * 4)));
    if (rgba == nullptr) {
        return {};
    }
    FillGrid(rgba, size);
    FPS2Texture tex = CreateFromAlignedRgba(size, size, rgba);
    free(rgba);
    return tex;
}

void FPS2Texture::Bind() const {
    if (!Valid() || !Leon::PS2::GetGSContext().ready) {
        return;
    }

    auto& scene = Leon::PS2::GetSceneState();
    if (scene.BoundTextureVram == vramAddress_) {
        return;
    }

    texbuffer_t texbuf{};
    texbuf.width = bufferWidth_;
    texbuf.psm = GS_PSM_32;
    texbuf.address = vramAddress_;
    texbuf.info.width = draw_log2(width_);
    texbuf.info.height = draw_log2(height_);
    // RGB: ignore texel alpha (ATEST NOTEQUAL 0 can punch holes with bad A).
    texbuf.info.components = TEXTURE_COMPONENTS_RGB;
    texbuf.info.function = TEXTURE_FUNCTION_MODULATE;

    lod_t lod{};
    lod.calculation = LOD_USE_K;
    lod.max_level = 0;
    lod.mag_filter = LOD_MAG_LINEAR;
    lod.min_filter = LOD_MIN_LINEAR;
    lod.l = 0;
    lod.k = 0;

    clutbuffer_t clut{};
    clut.storage_mode = CLUT_STORAGE_MODE1;
    clut.start = 0;
    clut.psm = 0;
    clut.load_method = CLUT_NO_LOAD;
    clut.address = 0;

    packet_t*& packet = BindPacketSlot();
    if (packet == nullptr) {
        packet = packet_init(16, PACKET_NORMAL);
        if (packet == nullptr) {
            return;
        }
    }
    qword_t* q = packet->data;
    q = draw_texture_sampling(q, 0, &lod);
    q = draw_texturebuffer(q, 0, &texbuf, &clut);
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();
    scene.BoundTextureVram = vramAddress_;
}

