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


[[nodiscard]] int NextPow2(int V) {
    int P = 1;
    while (P < V) {
        P <<= 1;
    }
    return P;
}

void FillChecker(unsigned char* Rgba, int Size) {
    for (int Y = 0; Y < Size; ++Y) {
        for (int X = 0; X < Size; ++X) {
            const int Cell = ((X >> 3) ^ (Y >> 3)) & 1;
            const unsigned char C = Cell ? static_cast<unsigned char>(220) : static_cast<unsigned char>(40);
            const int I = (Y * Size + X) * 4;
            Rgba[I + 0] = C;
            Rgba[I + 1] = C;
            Rgba[I + 2] = Cell ? static_cast<unsigned char>(200) : static_cast<unsigned char>(50);
            Rgba[I + 3] = 255;
        }
    }
}

void FillGrid(unsigned char* Rgba, int Size) {
    for (int Y = 0; Y < Size; ++Y) {
        for (int X = 0; X < Size; ++X) {
            const bool bLine = (X & 7) == 0 || (Y & 7) == 0;
            const int I = (Y * Size + X) * 4;
            Rgba[I + 0] = bLine ? static_cast<unsigned char>(70) : static_cast<unsigned char>(160);
            Rgba[I + 1] = bLine ? static_cast<unsigned char>(90) : static_cast<unsigned char>(170);
            Rgba[I + 2] = bLine ? static_cast<unsigned char>(80) : static_cast<unsigned char>(150);
            Rgba[I + 3] = 255;
        }
    }
}

[[nodiscard]] bool UploadRgba(int InVramAddress, int InBufferWidth, int InWidth, int InHeight,
                              void* RgbaAligned) {
    packet_t* LocalPacket = packet_init(80, PACKET_NORMAL);
    if (LocalPacket == nullptr) {
        return false;
    }
    qword_t* Q = LocalPacket->data;
    Q = draw_texture_transfer(Q, RgbaAligned, InWidth, InHeight, GS_PSM_32, InVramAddress, InBufferWidth);
    Q = draw_texture_flush(Q);
    dma_channel_send_chain(DMA_CHANNEL_GIF, LocalPacket->data, Q - LocalPacket->data, 0, 0);
    dma_wait_fast();
    packet_free(LocalPacket);
    return true;
}

packet_t*& BindPacketSlot() {
    static packet_t* Packet = nullptr;
    return Packet;
}


} // namespace

FPS2Texture FPS2Texture::CreateFromAlignedRgba(int InWidth, int InHeight, unsigned char* Rgba) {
    if (!Leon::PS2::GetGSContext().bReady || InWidth <= 0 || InHeight <= 0 || Rgba == nullptr) {
        return {};
    }
    const int BufW = NextPow2(InWidth);
    const int Addr = Leon::PS2::AllocateVram(BufW, InHeight, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    if (Addr < 0) {
        return {};
    }
    if (!UploadRgba(Addr, BufW, InWidth, InHeight, Rgba)) {
        return {};
    }
    return FPS2Texture(InWidth, InHeight, Addr, BufW);
}

FPS2Texture::~FPS2Texture() {
    Destroy();
}

FPS2Texture::FPS2Texture(FPS2Texture&& Other) noexcept
    : Width(Other.Width), Height(Other.Height), VramAddress(Other.VramAddress),
      BufferWidth(Other.BufferWidth) {
    Other.Width = 0;
    Other.Height = 0;
    Other.VramAddress = 0;
    Other.BufferWidth = 0;
}

FPS2Texture& FPS2Texture::operator=(FPS2Texture&& Other) noexcept {
    if (this != &Other) {
        Destroy();
        Width = Other.Width;
        Height = Other.Height;
        VramAddress = Other.VramAddress;
        BufferWidth = Other.BufferWidth;
        Other.Width = 0;
        Other.Height = 0;
        Other.VramAddress = 0;
        Other.BufferWidth = 0;
    }
    return *this;
}

void FPS2Texture::Destroy() {
    if (VramAddress != 0) {
        Leon::PS2::InvalidateBoundTexture();
    }
    Width = 0;
    Height = 0;
    VramAddress = 0;
    BufferWidth = 0;
}

bool FPS2Texture::Valid() const {
    return Width > 0 && Height > 0 && VramAddress > 0;
}

FPS2Texture FPS2Texture::Create(int InWidth, int InHeight, const unsigned char* Rgba) {
    if (Rgba == nullptr || InWidth <= 0 || InHeight <= 0) {
        return {};
    }
    auto* Aligned = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(InWidth * InHeight * 4)));
    if (Aligned == nullptr) {
        return {};
    }
    std::memcpy(Aligned, Rgba, static_cast<size_t>(InWidth * InHeight * 4));
    FPS2Texture Tex = CreateFromAlignedRgba(InWidth, InHeight, Aligned);
    free(Aligned);
    return Tex;
}

FPS2Texture FPS2Texture::CreateChecker(int Size) {
    if (Size < 8) {
        Size = 8;
    }
    auto* Rgba = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(Size * Size * 4)));
    if (Rgba == nullptr) {
        return {};
    }
    FillChecker(Rgba, Size);
    FPS2Texture Tex = CreateFromAlignedRgba(Size, Size, Rgba);
    free(Rgba);
    return Tex;
}

FPS2Texture FPS2Texture::CreateGrid(int Size) {
    if (Size < 8) {
        Size = 8;
    }
    auto* Rgba = static_cast<unsigned char*>(memalign(16, static_cast<size_t>(Size * Size * 4)));
    if (Rgba == nullptr) {
        return {};
    }
    FillGrid(Rgba, Size);
    FPS2Texture Tex = CreateFromAlignedRgba(Size, Size, Rgba);
    free(Rgba);
    return Tex;
}

void FPS2Texture::Bind() const {
    if (!Valid() || !Leon::PS2::GetGSContext().bReady) {
        return;
    }

    auto& Scene = Leon::PS2::GetSceneState();
    if (Scene.BoundTextureVram == VramAddress) {
        return;
    }

    texbuffer_t Texbuf{};
    Texbuf.width = BufferWidth;
    Texbuf.psm = GS_PSM_32;
    Texbuf.address = VramAddress;
    Texbuf.info.width = draw_log2(Width);
    Texbuf.info.height = draw_log2(Height);
    // RGB: ignore texel alpha (ATEST NOTEQUAL 0 can punch holes with bad A).
    Texbuf.info.components = TEXTURE_COMPONENTS_RGB;
    Texbuf.info.function = TEXTURE_FUNCTION_MODULATE;

    lod_t Lod{};
    Lod.calculation = LOD_USE_K;
    Lod.max_level = 0;
    Lod.mag_filter = LOD_MAG_LINEAR;
    Lod.min_filter = LOD_MIN_LINEAR;
    Lod.l = 0;
    Lod.k = 0;

    clutbuffer_t Clut{};
    Clut.storage_mode = CLUT_STORAGE_MODE1;
    Clut.start = 0;
    Clut.psm = 0;
    Clut.load_method = CLUT_NO_LOAD;
    Clut.address = 0;

    packet_t*& LocalPacket = BindPacketSlot();
    if (LocalPacket == nullptr) {
        LocalPacket = packet_init(16, PACKET_NORMAL);
        if (LocalPacket == nullptr) {
            return;
        }
    }
    qword_t* Q = LocalPacket->data;
    Q = draw_texture_sampling(Q, 0, &Lod);
    Q = draw_texturebuffer(Q, 0, &Texbuf, &Clut);
    dma_channel_send_normal(DMA_CHANNEL_GIF, LocalPacket->data, Q - LocalPacket->data, 0, 0);
    dma_wait_fast();
    Scene.BoundTextureVram = VramAddress;
}

