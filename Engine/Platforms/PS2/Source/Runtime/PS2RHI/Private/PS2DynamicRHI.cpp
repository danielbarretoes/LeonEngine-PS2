#include "PS2GSContext.h"

#include "DynamicRHI.h"
#include "PS2RHI.h"

#include <cstdio>
#include <memory>

#include <dma.h>
#include <graph.h>
#include <gs_psm.h>

namespace {


// Textured box (~60 qwords) + batched HUD rects share this packet.
constexpr int kPacketQwords = 2048;

bool SetupDrawingEnvironment(Leon::PS2::FPS2GSContext& gs) {
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

void ClearFramebuffer(Leon::PS2::FPS2GSContext& gs, int r, int g, int b) {
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

/** PS2 Graphics Synthesizer backend (UE: F<Platform>DynamicRHI). */
class FPS2DynamicRHI final : public FDynamicRHI
{
public:
	virtual bool Init(void* (*)(const char*)) override
	{
		Version = "GS";
		std::printf("Leon RHI: PS2 %s\n", Version);
		return true;
	}

	virtual void SetViewport(int32, int32, int32 Width, int32 Height) override
	{
		ScreenWidth = Width > 0 ? Width : 640;
		ScreenHeight = Height > 0 ? Height : 448;
		if (Leon::PS2::GetGSContext().ready)
		{
			graph_set_screen(0, 0, ScreenWidth, ScreenHeight);
		}
	}

	virtual FRHIGPUMemoryStats GetGPUMemoryStats() const override
	{
		FRHIGPUMemoryStats Stats;
		Stats.bValid = true;
		Stats.bReportsUsage = true;
		Stats.BudgetBytes = 4u * 1024u * 1024u;
		Stats.UsedBytes = static_cast<uint64>(Leon::PS2::GetGSContext().vramEndWords) * 4u;
		return Stats;
	}

	virtual const char* GetName() const override
	{
		return "PS2";
	}

	virtual const char* GetAPIVersionString() const override
	{
		return Version;
	}

	void Clear(float r, float g, float b)
	{
		auto& gs = Leon::PS2::GetGSContext();
		const int rr = static_cast<int>(r * 255.0f) & 0xFF;
		const int gg = static_cast<int>(g * 255.0f) & 0xFF;
		const int bb = static_cast<int>(b * 255.0f) & 0xFF;
		graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
			static_cast<unsigned char>(bb));
		ClearFramebuffer(gs, rr, gg, bb);
	}

private:
	const char* Version = "unknown";
	int32 ScreenWidth = 640;
	int32 ScreenHeight = 448;
};

FPS2DynamicRHI* GPS2DynamicRHI = nullptr;

} // namespace

bool FPS2RHI::InitDisplay(int width, int height) {
    auto& gs = Leon::PS2::GetGSContext();
    const int w = width > 0 ? width : 640;
    const int h = height > 0 ? height : 448;

    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    gs.frame.width = w;
    gs.frame.height = h;
    gs.frame.mask = 0;
    gs.frame.psm = GS_PSM_32;
    const int frameVram = Leon::PS2::AllocateVram(w, h, GS_PSM_32, GRAPH_ALIGN_PAGE);
    if (frameVram < 0) {
        std::printf("FPS2RHI::InitDisplay: frame VRAM allocate failed\n");
        return false;
    }
    gs.frame.address = static_cast<unsigned int>(frameVram);

    const int zVram = Leon::PS2::AllocateVram(w, h, GS_ZBUF_32, GRAPH_ALIGN_PAGE);
    if (zVram < 0) {
        std::printf("FPS2RHI::InitDisplay: z-buffer VRAM allocate failed\n");
        return false;
    }
    gs.z.enable = DRAW_ENABLE;
    gs.z.mask = 0;
    gs.z.method = ZTEST_METHOD_GREATER_EQUAL;
    gs.z.zsm = GS_ZBUF_32;
    gs.z.address = static_cast<unsigned int>(zVram);

    if (graph_initialize(gs.frame.address, w, h, GS_PSM_32, 0, 0) < 0) {
        std::printf("FPS2RHI::InitDisplay: graph_initialize failed\n");
        return false;
    }

    if (!SetupDrawingEnvironment(gs)) {
        std::printf("FPS2RHI::InitDisplay: draw environment failed\n");
        return false;
    }

    graph_set_bgcolor(0x20, 0x50, 0xC0);
    ClearFramebuffer(gs, 0x20, 0x50, 0xC0);
    graph_enable_output();
    graph_wait_vsync();

    gs.ready = true;
    std::printf("FPS2RHI::InitDisplay: %dx%d GS + z-buffer ready\n", w, h);
    return true;
}

void FPS2RHI::WaitVSync() {
    if (Leon::PS2::GetGSContext().ready) {
        graph_wait_vsync();
    }
}

std::unique_ptr<FDynamicRHI> PlatformCreateDynamicRHI() {
    auto device = std::make_unique<FPS2DynamicRHI>();
    GPS2DynamicRHI = device.get();
    return device;
}

void FPS2RHI::ClearColor(float r, float g, float b) {
    if (GPS2DynamicRHI != nullptr) {
        GPS2DynamicRHI->Clear(r, g, b);
        return;
    }
    auto& gs = Leon::PS2::GetGSContext();
    if (gs.packet != nullptr) {
        const int rr = static_cast<int>(r * 255.0f) & 0xFF;
        const int gg = static_cast<int>(g * 255.0f) & 0xFF;
        const int bb = static_cast<int>(b * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(rr), static_cast<unsigned char>(gg),
                          static_cast<unsigned char>(bb));
        ClearFramebuffer(gs, rr, gg, bb);
    }
}

