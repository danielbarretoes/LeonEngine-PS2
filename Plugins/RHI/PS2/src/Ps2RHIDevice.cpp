#include "Ps2GsInternal.h"

#include <leon/rhi/IRHIDevice.h>
#include <leon/rhi/Ps2RHI.h>

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
using ps2gs::g_displayReady;
using ps2gs::g_frame;
using ps2gs::g_packet;
using ps2gs::g_z;
using ps2gs::OriginX;
using ps2gs::OriginY;

bool SetupDrawingEnvironment() {
    if (g_packet == nullptr) {
        g_packet = packet_init(128, PACKET_NORMAL);
        if (g_packet == nullptr) {
            return false;
        }
    }

    qword_t* q = g_packet->data;
    q = draw_setup_environment(q, 0, &g_frame, &g_z);
    q = draw_primitive_xyoffset(q, 0, OriginX(), OriginY());
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, g_packet->data, q - g_packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

void ClearFramebuffer(int r, int g, int b) {
    if (g_packet == nullptr) {
        return;
    }

    qword_t* q = g_packet->data;
    q = draw_clear(q, 0, OriginX(), OriginY(), static_cast<float>(g_frame.width),
                   static_cast<float>(g_frame.height), r, g, b);
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, g_packet->data, q - g_packet->data, 0, 0);
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
        if (g_displayReady) {
            graph_set_screen(0, 0, width_, height_);
        }
#endif
    }

    [[nodiscard]] GpuMemoryInfo QueryGpuMemory() const override {
        GpuMemoryInfo out{};
        out.valid = true;
        out.reportsUsage = false;
        out.budgetBytes = 4u * 1024u * 1024u;
        out.usedBytes = 0;
        return out;
    }

    [[nodiscard]] const char* GetName() const override { return "PS2"; }

    [[nodiscard]] const char* GetApiVersionString() const override { return version_; }

    void Clear(float r, float g, float b) {
#if defined(LEON_PLATFORM_PS2)
        const int rr = static_cast<int>(r * 255.0f) & 0xFF;
        const int gg = static_cast<int>(g * 255.0f) & 0xFF;
        const int bb = static_cast<int>(b * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
                          static_cast<unsigned char>(bb));
        ClearFramebuffer(rr, gg, bb);
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
    const int w = width > 0 ? width : 640;
    const int h = height > 0 ? height : 448;

    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    g_frame.width = w;
    g_frame.height = h;
    g_frame.mask = 0;
    g_frame.psm = GS_PSM_32;
    const int vram = graph_vram_allocate(w, h, GS_PSM_32, GRAPH_ALIGN_PAGE);
    if (vram < 0) {
        std::printf("Ps2InitDisplay: graph_vram_allocate failed\n");
        return false;
    }
    g_frame.address = static_cast<unsigned int>(vram);

    g_z.enable = DRAW_DISABLE;
    g_z.mask = 0;
    g_z.method = ZTEST_METHOD_ALLPASS;
    g_z.zsm = GS_ZBUF_32;
    g_z.address = 0;

    if (graph_initialize(g_frame.address, w, h, GS_PSM_32, 0, 0) < 0) {
        std::printf("Ps2InitDisplay: graph_initialize failed\n");
        return false;
    }

    if (!SetupDrawingEnvironment()) {
        std::printf("Ps2InitDisplay: draw environment failed\n");
        return false;
    }

    graph_set_bgcolor(0x20, 0x50, 0xC0);
    ClearFramebuffer(0x20, 0x50, 0xC0);
    graph_enable_output();
    graph_wait_vsync();

    g_displayReady = true;
    std::printf("Ps2InitDisplay: %dx%d GS ready\n", w, h);
    return true;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

void Ps2WaitVsync() {
#if defined(LEON_PLATFORM_PS2)
    if (g_displayReady) {
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
    }
#if defined(LEON_PLATFORM_PS2)
    else if (g_packet != nullptr) {
        const int rr = static_cast<int>(r * 255.0f) & 0xFF;
        const int gg = static_cast<int>(g * 255.0f) & 0xFF;
        const int bb = static_cast<int>(b * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
                          static_cast<unsigned char>(bb));
        ClearFramebuffer(rr, gg, bb);
    }
#else
    (void)r;
    (void)g;
    (void)b;
#endif
}

} // namespace leon::rhi
