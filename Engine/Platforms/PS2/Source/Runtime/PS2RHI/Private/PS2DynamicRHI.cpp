#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"
#include "PS2SceneState.h"

#include <dma.h>
#include <graph.h>
#include <gs_psm.h>

namespace
{

	/** libgraph's pixel format for an allocation (the Z formats have their own codes). */
	[[nodiscard]] int GraphPsm(EGSPixelFormat Format)
	{
		return int(Format);
	}

	/** A full-screen sprite of Color that writes Z 0 (the farthest), whatever the depth test. */
	void AppendClear(Leon::PS2::FPS2GSContext& Gs, const FGSRGBAQ& Color)
	{
		const float HalfWidth = float(Gs.Width) * 0.5f;
		const float HalfHeight = float(Gs.Height) * 0.5f;
		Leon::PS2::AppendDepthTest(Gs, false);
		FGSPrim Sprite;
		Sprite.Type = EGSPrimitive::Sprite;
		Gs.FrameList.SetPrim(Sprite);
		Gs.FrameList.SetRGBAQ(Color);
		Gs.FrameList.AddVertex(Leon::PS2::ScreenVertex(-HalfWidth, -HalfHeight));
		Gs.FrameList.AddVertex(Leon::PS2::ScreenVertex(HalfWidth, HalfHeight));
		Leon::PS2::AppendDepthTest(Gs, true);
	}

	/** PS2 Graphics Synthesizer backend (UE: F<Platform>DynamicRHI). */
	class FPS2DynamicRHI final : public FDynamicRHI
	{
	public:
		virtual bool Init(void* (*)(const char*)) override
		{
			UE_LOG(LogRHI, Log, "PS2 GS");
			return true;
		}

		virtual void SetViewport(int32, int32, int32 Width, int32 Height) override
		{
			if (Leon::PS2::GetGSContext().bReady)
			{
				graph_set_screen(0, 0, Width > 0 ? Width : 640, Height > 0 ? Height : 448);
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
			return "GS";
		}
	};

} // namespace

bool FPS2RHI::InitDisplay(int Width, int Height, EGSPixelFormat ColorFormat, uint32 ReservedVramBytes)
{
	auto& Gs = Leon::PS2::GetGSContext();
	check(ColorFormat == EGSPixelFormat::PSMCT32 || ColorFormat == EGSPixelFormat::PSMCT16S);
	Gs.Width = Width > 0 ? Width : 640;
	Gs.Height = Height > 0 ? Height : 448;

	dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	// The caller's region first (GSConformance: its scenes' local memory), then two frame buffers and the Z buffer.
	if (ReservedVramBytes > 0 &&
		Leon::PS2::AllocateVram(64, int32(ReservedVramBytes / 256), GS_PSM_32, GRAPH_ALIGN_PAGE) < 0)
	{
		UE_LOG(LogRHI, Error, "FPS2RHI::InitDisplay: cannot reserve %u bytes of VRAM", ReservedVramBytes);
		return false;
	}
	for (FGSFrame& Frame : Gs.Frames)
	{
		const int32 Address = Leon::PS2::AllocateVram(Gs.Width, Gs.Height, GraphPsm(ColorFormat), GRAPH_ALIGN_PAGE);
		if (Address < 0)
		{
			UE_LOG(LogRHI, Error, "FPS2RHI::InitDisplay: frame buffer VRAM allocation failed");
			return false;
		}
		Frame.FBP = uint16(Address / 2048);
		Frame.FBW = uint8((Gs.Width + 63) / 64);
		Frame.PSM = ColorFormat;
	}
	const int32 ZAddress =
		Leon::PS2::AllocateVram(Gs.Width, Gs.Height, GraphPsm(EGSPixelFormat::PSMZ24), GRAPH_ALIGN_PAGE);
	if (ZAddress < 0)
	{
		UE_LOG(LogRHI, Error, "FPS2RHI::InitDisplay: Z buffer VRAM allocation failed");
		return false;
	}
	Gs.ZBuf.ZBP = uint16(ZAddress / 2048);
	Gs.ZBuf.PSM = EGSPixelFormat::PSMZ24;

	// The CRTC shows Frames[1] while the GS draws Frames[0].
	Gs.BackBuffer = 0;
	if (graph_initialize(Gs.Frames[1].FBP * 2048, Gs.Width, Gs.Height, GraphPsm(ColorFormat), 0, 0) < 0)
	{
		UE_LOG(LogRHI, Error, "FPS2RHI::InitDisplay: graph_initialize failed");
		return false;
	}
	Gs.bReady = true;
	Leon::PS2::AppendDrawEnvironment(Gs);
	ClearColor(0.125f, 0.3125f, 0.75f);
	WaitVSync();
	UE_LOG(LogRHI, Log, "FPS2RHI::InitDisplay: %dx%d, %s color, Z24, double buffered", Gs.Width, Gs.Height,
		ColorFormat == EGSPixelFormat::PSMCT32 ? "32-bit" : "16-bit dithered");
	return true;
}

void FPS2RHI::ClearColor(float R, float G, float B)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (Gs.bReady)
	{
		AppendClear(Gs, Leon::PS2::UnitColor(R, G, B));
	}
}

void FPS2RHI::Submit(const FGSCommandList& List)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return;
	}
	Gs.FrameList.Append(List);
	Leon::PS2::AppendDrawEnvironment(Gs);
	Leon::PS2::InvalidateBoundTexture();
}

FGSXYZ FPS2RHI::ScreenVertex(float X, float Y, uint32 Z)
{
	return Leon::PS2::ScreenVertex(X, Y, Z);
}

FGSDrawEnvironment FPS2RHI::GetDrawEnvironment()
{
	return Leon::PS2::GetDrawEnvironment(Leon::PS2::GetGSContext());
}

bool FPS2RHI::AllocateTextureArena(uint32& OutFirstBlock, uint32& OutNumBlocks)
{
	// The VRAM is 1 M words; the arena is every page after the allocator's end, taken as one 64-texel-wide buffer.
	constexpr int32 VramWords = 1024 * 1024;
	constexpr int32 PageWords = 2048;
	auto& Gs = Leon::PS2::GetGSContext();
	const int32 FirstWord = ((Gs.VramEndWords + PageWords - 1) / PageWords) * PageWords;
	const int32 NumWords = VramWords - FirstWord;
	if (!Gs.bReady || NumWords < PageWords ||
		Leon::PS2::AllocateVram(64, NumWords / 64, GS_PSM_32, GRAPH_ALIGN_PAGE) != FirstWord)
	{
		return false;
	}
	OutFirstBlock = uint32(FirstWord / 64);
	OutNumBlocks = uint32(NumWords / 64);
	UE_LOG(LogRHI, Log, "FPS2RHI: texture arena of %d KB", NumWords * 4 / 1024);
	return true;
}

void FPS2RHI::WaitVSync()
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return;
	}
	Leon::PS2::FlushFrame(Gs);
	graph_wait_vsync();
	// With an interval of N, the blanks before the N-th since the last flip pass too: the clock tells which blank this
	// is (half a field of margin), since libgraph counts none.
	constexpr double FieldSeconds = 1.0 / 59.94;
	while (Gs.SyncInterval > 1 &&
		FPlatformTime::Seconds() - Gs.LastFlipSeconds < (double(Gs.SyncInterval) - 0.5) * FieldSeconds)
	{
		graph_wait_vsync();
	}
	Gs.LastFlipSeconds = FPlatformTime::Seconds();
	const FGSFrame& Drawn = Gs.Frames[Gs.BackBuffer];
	graph_set_framebuffer_filtered(Drawn.FBP * 2048, Gs.Width, GraphPsm(Drawn.PSM), 0, 0);
	Gs.BackBuffer ^= 1;
	Leon::PS2::AppendDrawEnvironment(Gs);
}

void FPS2RHI::SetSyncInterval(int32 Interval)
{
	Leon::PS2::GetGSContext().SyncInterval = FMath::Clamp(Interval, 1, 4);
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	return new FPS2DynamicRHI();
}
