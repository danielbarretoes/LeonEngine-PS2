#include "Ps2GsContext.h"

#include "IRHIDevice.h"
#include "Ps2RHI.h"

#include <cstdio>
#include <memory>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <graph.h>
#include <gs_psm.h>
#endif

namespace leon::rhi {
namespace {

#if defined(LEON_PLATFORM_PS2)

// Textured box (~60 qwords) + batched HUD rects share this packet.
constexpr int kPacketQwords = 2048;

bool SetupDrawingEnvironment(ps2::GsContext& gs) {
    if (gs.packet == nullptr) {
        gs.packet = packet_init(kPacketQwords, PACKET_NORMAL);
        if (gs.packet == nullptr) {
            return false;
        }
    }

    qword_t* q = gs.packet->data;
    q = draw_setup_environment(q, 0, &gs.frame, &gs.z);
    // draw_setup_environment already programs SCISSOR in window space (0..w/h).
    // Do not override with a second scissor — wrong coords blank the screen.
    q = draw_primitive_xyoffset(q, 0, gs.OriginX(), gs.OriginY());
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, q - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

void ClearFramebuffer(ps2::GsContext& gs, int r, int g, int b) {
    if (gs.packet == nullptr) {
        return;
    }

    // Disable z-test while clearing so the color fill always lands; z writes 0.
    qword_t* q = gs.packet->data;
    q = draw_disable_tests(q, 0, &gs.z);
    q = draw_clear(q, 0, gs.OriginX(), gs.OriginY(), static_cast<float>(gs.frame.width),
                   static_cast<float>(gs.frame.height), r, g, b);
    q = draw_enable_tests(q, 0, &gs.z);
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, q - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
}
#endif

class Ps2RHIDevice final : public IRHIDevice {
public:
    bool LoadProcedures(void* (* /*loader*/)(const char*)) override {
#if defined(LEON_PLATFORM_PS2)
        version_ = "GS";
        std::printf("Leon RHI: PS2 %s\n", version_);
        return true;
#else
        return false;
#endif
    }

    void SetViewport(int /*x*/, int /*y*/, int width, int height) override {
        width_ = width > 0 ? width : 640;
        height_ = height > 0 ? height : 448;
#if defined(LEON_PLATFORM_PS2)
        if (ps2::GetGsContext().ready) {
            graph_set_screen(0, 0, width_, height_);
        }
#endif
    }

    [[nodiscard]] GpuMemoryInfo QueryGpuMemory() const override {
        GpuMemoryInfo out{};
        out.valid = true;
        out.budgetBytes = 4u * 1024u * 1024u;
#if defined(LEON_PLATFORM_PS2)
        out.reportsUsage = true;
        out.usedBytes = static_cast<std::size_t>(ps2::GetGsContext().vramEndWords) * 4u;
#else
        out.reportsUsage = false;
        out.usedBytes = 0;
#endif
        return out;
    }

    [[nodiscard]] const char* GetName() const override { return "PS2"; }

    [[nodiscard]] const char* GetApiVersionString() const override { return version_; }

    void Clear(float r, float g, float b) {
#if defined(LEON_PLATFORM_PS2)
        auto& gs = ps2::GetGsContext();
        const int rr = static_cast<int>(r * 255.0f) & 0xFF;
        const int gg = static_cast<int>(g * 255.0f) & 0xFF;
        const int bb = static_cast<int>(b * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
                          static_cast<unsigned char>(bb));
        ClearFramebuffer(gs, rr, gg, bb);
#else
        (void)r;
        (void)g;
        (void)b;
#endif
    }

private:
    const char* version_ = "unknown";
    int width_ = 640;
    int height_ = 448;
};

Ps2RHIDevice* g_ps2Device = nullptr;

[[nodiscard]] IRHIDevice*& ActiveDeviceSlot() {
    static IRHIDevice* active = nullptr;
    return active;
}

} // namespace

bool Ps2InitDisplay(int width, int height) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    const int w = width > 0 ? width : 640;
    const int h = height > 0 ? height : 448;

    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    gs.frame.width = w;
    gs.frame.height = h;
    gs.frame.mask = 0;
    gs.frame.psm = GS_PSM_32;
    const int frameVram = ps2::AllocateVram(w, h, GS_PSM_32, GRAPH_ALIGN_PAGE);
    if (frameVram < 0) {
        std::printf("Ps2InitDisplay: frame VRAM allocate failed\n");
        return false;
    }
    gs.frame.address = static_cast<unsigned int>(frameVram);

    const int zVram = ps2::AllocateVram(w, h, GS_ZBUF_32, GRAPH_ALIGN_PAGE);
    if (zVram < 0) {
        std::printf("Ps2InitDisplay: z-buffer VRAM allocate failed\n");
        return false;
    }
    gs.z.enable = DRAW_ENABLE;
    gs.z.mask = 0;
    gs.z.method = ZTEST_METHOD_GREATER_EQUAL;
    gs.z.zsm = GS_ZBUF_32;
    gs.z.address = static_cast<unsigned int>(zVram);

    if (graph_initialize(gs.frame.address, w, h, GS_PSM_32, 0, 0) < 0) {
        std::printf("Ps2InitDisplay: graph_initialize failed\n");
        return false;
    }

    if (!SetupDrawingEnvironment(gs)) {
        std::printf("Ps2InitDisplay: draw environment failed\n");
        return false;
    }

    graph_set_bgcolor(0x20, 0x50, 0xC0);
    ClearFramebuffer(gs, 0x20, 0x50, 0xC0);
    graph_enable_output();
    graph_wait_vsync();

    gs.ready = true;
    std::printf("Ps2InitDisplay: %dx%d GS + z-buffer ready\n", w, h);
    return true;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

void Ps2WaitVsync() {
#if defined(LEON_PLATFORM_PS2)
    if (ps2::GetGsContext().ready) {
        graph_wait_vsync();
    }
#endif
}

std::unique_ptr<IRHIDevice> CreateDefaultRHIDevice() {
    auto device = std::make_unique<Ps2RHIDevice>();
    g_ps2Device = device.get();
    return device;
}

void SetActiveDevice(IRHIDevice* device) {
    ActiveDeviceSlot() = device;
}

IRHIDevice* GetActiveDevice() {
    return ActiveDeviceSlot();
}

void Ps2ClearColor(float r, float g, float b) {
    if (g_ps2Device != nullptr) {
        g_ps2Device->Clear(r, g, b);
        return;
    }
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (gs.packet != nullptr) {
        const int rr = static_cast<int>(r * 255.0f) & 0xFF;
        const int gg = static_cast<int>(g * 255.0f) & 0xFF;
        const int bb = static_cast<int>(b * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
                          static_cast<unsigned char>(bb));
        ClearFramebuffer(gs, rr, gg, bb);
    }
#else
    (void)r;
    (void)g;
    (void)b;
#endif
}

} // namespace leon::rhi
