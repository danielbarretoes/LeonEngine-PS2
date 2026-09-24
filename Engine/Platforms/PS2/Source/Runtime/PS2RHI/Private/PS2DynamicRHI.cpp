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
constexpr int PacketQwords = 2048;

bool SetupDrawingEnvironment(Leon::PS2::FPS2GSContext& Gs) {
    if (Gs.Packet == nullptr) {
        Gs.Packet = packet_init(PacketQwords, PACKET_NORMAL);
        if (Gs.Packet == nullptr) {
            return false;
        }
    }

    qword_t* Q = Gs.Packet->data;
    Q = draw_setup_environment(Q, 0, &Gs.Frame, &Gs.Z);
    // draw_setup_environment already programs SCISSOR in window space (0..w/h).
    // Do not override with a second scissor — wrong coords blank the screen.
    Q = draw_primitive_xyoffset(Q, 0, Gs.OriginX(), Gs.OriginY());
    Q = draw_finish(Q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, Gs.Packet->data, Q - Gs.Packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

void ClearFramebuffer(Leon::PS2::FPS2GSContext& Gs, int R, int G, int B) {
    if (Gs.Packet == nullptr) {
        return;
    }

    // Disable z-test while clearing so the color fill always lands; z writes 0.
    qword_t* Q = Gs.Packet->data;
    Q = draw_disable_tests(Q, 0, &Gs.Z);
    Q = draw_clear(Q, 0, Gs.OriginX(), Gs.OriginY(), static_cast<float>(Gs.Frame.width),
                   static_cast<float>(Gs.Frame.height), R, G, B);
    Q = draw_enable_tests(Q, 0, &Gs.Z);
    Q = draw_finish(Q);

    dma_channel_send_normal(DMA_CHANNEL_GIF, Gs.Packet->data, Q - Gs.Packet->data, 0, 0);
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
		if (Leon::PS2::GetGSContext().bReady)
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
		Stats.UsedBytes = static_cast<uint64>(Leon::PS2::GetGSContext().VramEndWords) * 4u;
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

	void Clear(float R, float G, float B)
	{
		auto& Gs = Leon::PS2::GetGSContext();
		const int Rr = static_cast<int>(R * 255.0f) & 0xFF;
		const int Gg = static_cast<int>(G * 255.0f) & 0xFF;
		const int Bb = static_cast<int>(B * 255.0f) & 0xFF;
		graph_set_bgcolor(static_cast<unsigned char>(Rr), static_cast<unsigned char>(Gg),
			static_cast<unsigned char>(Bb));
		ClearFramebuffer(Gs, Rr, Gg, Bb);
	}

private:
	const char* Version = "unknown";
	int32 ScreenWidth = 640;
	int32 ScreenHeight = 448;
};

FPS2DynamicRHI* GPS2DynamicRHI = nullptr;

} // namespace

bool FPS2RHI::InitDisplay(int Width, int Height) {
    auto& Gs = Leon::PS2::GetGSContext();
    const int W = Width > 0 ? Width : 640;
    const int H = Height > 0 ? Height : 448;

    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    Gs.Frame.width = W;
    Gs.Frame.height = H;
    Gs.Frame.mask = 0;
    Gs.Frame.psm = GS_PSM_32;
    const int FrameVram = Leon::PS2::AllocateVram(W, H, GS_PSM_32, GRAPH_ALIGN_PAGE);
    if (FrameVram < 0) {
        std::printf("FPS2RHI::InitDisplay: frame VRAM allocate failed\n");
        return false;
    }
    Gs.Frame.address = static_cast<unsigned int>(FrameVram);

    const int ZVram = Leon::PS2::AllocateVram(W, H, GS_ZBUF_32, GRAPH_ALIGN_PAGE);
    if (ZVram < 0) {
        std::printf("FPS2RHI::InitDisplay: z-buffer VRAM allocate failed\n");
        return false;
    }
    Gs.Z.enable = DRAW_ENABLE;
    Gs.Z.mask = 0;
    Gs.Z.method = ZTEST_METHOD_GREATER_EQUAL;
    Gs.Z.zsm = GS_ZBUF_32;
    Gs.Z.address = static_cast<unsigned int>(ZVram);

    if (graph_initialize(Gs.Frame.address, W, H, GS_PSM_32, 0, 0) < 0) {
        std::printf("FPS2RHI::InitDisplay: graph_initialize failed\n");
        return false;
    }

    if (!SetupDrawingEnvironment(Gs)) {
        std::printf("FPS2RHI::InitDisplay: draw environment failed\n");
        return false;
    }

    graph_set_bgcolor(0x20, 0x50, 0xC0);
    ClearFramebuffer(Gs, 0x20, 0x50, 0xC0);
    graph_enable_output();
    graph_wait_vsync();

    Gs.bReady = true;
    std::printf("FPS2RHI::InitDisplay: %dx%d GS + z-buffer ready\n", W, H);
    return true;
}

void FPS2RHI::WaitVSync() {
    if (Leon::PS2::GetGSContext().bReady) {
        graph_wait_vsync();
    }
}

std::unique_ptr<FDynamicRHI> PlatformCreateDynamicRHI() {
    auto Device = std::make_unique<FPS2DynamicRHI>();
    GPS2DynamicRHI = Device.get();
    return Device;
}

void FPS2RHI::ClearColor(float R, float G, float B) {
    if (GPS2DynamicRHI != nullptr) {
        GPS2DynamicRHI->Clear(R, G, B);
        return;
    }
    auto& Gs = Leon::PS2::GetGSContext();
    if (Gs.Packet != nullptr) {
        const int Rr = static_cast<int>(R * 255.0f) & 0xFF;
        const int Gg = static_cast<int>(G * 255.0f) & 0xFF;
        const int Bb = static_cast<int>(B * 255.0f) & 0xFF;
        graph_set_bgcolor(static_cast<unsigned char>(Rr), static_cast<unsigned char>(Gg),
                          static_cast<unsigned char>(Bb));
        ClearFramebuffer(Gs, Rr, Gg, Bb);
    }
}

