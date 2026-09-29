#include "DynamicRHI.h"
#include "GSDebugDraw.h"
#include "GSFieldPacer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"
#include "PS2VU1.h"
#include "PS2VerticalBlank.h"

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

	/**
	 * The console's television mode: PAL when ROMVER's region letter is E (libgraph's graph_get_region), NTSC otherwise
	 * (J, A, C, H); -PAL or -NTSC on the command line chooses one.
	 */
	[[nodiscard]] EGSVideoMode GetVideoMode()
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		if (FParse::Param(CommandLine, TEXT("PAL")))
		{
			return EGSVideoMode::Pal;
		}
		if (FParse::Param(CommandLine, TEXT("NTSC")))
		{
			return EGSVideoMode::Ntsc;
		}
		return graph_get_region() == GRAPH_MODE_PAL ? EGSVideoMode::Pal : EGSVideoMode::Ntsc;
	}

	/**
	 * The CRTC's display area: Width x Height pixels, centred vertically in the mode's visible lines (a 448-line frame
	 * has 32 black lines above and below on PAL).
	 */
	void SetScreen(const Leon::PS2::FPS2GSContext& Gs, int32 Width, int32 Height)
	{
		const int32 Top = FMath::Max(0, (FGSFieldPacer::GetVisibleLines(Gs.VideoMode) - Height) / 2);
		graph_set_screen(0, Top, Width, Height);
	}

	/** A full-screen sprite of Color that writes Z 0 (the farthest), whatever the depth test. */
	void AppendClear(Leon::PS2::FPS2GSContext& Gs, const FGSRGBAQ& Color)
	{
		const FGSDrawEnvironment Environment = Leon::PS2::GetDrawEnvironment(Gs);
		FGSCommandList& List = Gs.GetFrameList();
		List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
		FGSPrim Sprite;
		Sprite.Type = EGSPrimitive::Sprite;
		List.SetPrim(Sprite);
		List.SetRGBAQ(Color);
		List.AddVertex(Environment.PixelVertex(0.0f, 0.0f));
		List.AddVertex(Environment.PixelVertex(float(Gs.Width), float(Gs.Height)));
		List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
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
			const Leon::PS2::FPS2GSContext& Gs = Leon::PS2::GetGSContext();
			if (Gs.bReady)
			{
				SetScreen(Gs, Width > 0 ? Width : 640, Height > 0 ? Height : 448);
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
	// From an empty VRAM each time: a second call (the boot error screen after a failed one, or after a fatal error in
	// the middle of a frame) waits for the frame in flight, then starts over.
	Leon::PS2::DropPendingFrame(Gs);
	Leon::PS2::BeginNextFrame(Gs);
	graph_vram_clear();
	Gs.VramEndWords = 0;
	Gs.bReady = false;
	for (FGSCommandList& List : Gs.FrameLists)
	{
		List.Reset();
	}
	Gs.FrameIndex = 0;

	// The GIF from its reset (GIF_CTRL.RST): what ran before the ELF may leave a PATH3 packet open in it, which would
	// hold the GIF (a path keeps it until its packet ends) and stall every DIRECT of the frames.
	*reinterpret_cast<volatile uint32*>(UPTRINT(0x10003000)) = 1;
	// The frame goes to VIF1 (its GS writes by PATH2, its vertex batches through VU1 and PATH1); the microprograms are
	// in VU1's micro memory before the first frame.
	dma_channel_initialize(DMA_CHANNEL_VIF1, nullptr, 0);
	if (FPS2VU1::IsEnabled())
	{
		FPS2VU1::UploadPrograms();
	}

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

	// The fields are counted from the vertical blank interrupt (once: a second call keeps the handler).
	if (!Leon::PS2::FPS2VerticalBlank::Install())
	{
		UE_LOG(LogRHI, Error, "FPS2RHI::InitDisplay: cannot install the vertical blank handler");
		return false;
	}

	// The CRTC in the console's mode, interlaced with the flicker filter (libgraph's graph_initialize, with the
	// region's frame centred), showing Frames[1] while the GS draws Frames[0].
	Gs.VideoMode = GetVideoMode();
	graph_set_mode(GRAPH_MODE_INTERLACED, Gs.VideoMode == EGSVideoMode::Pal ? GRAPH_MODE_PAL : GRAPH_MODE_NTSC,
		GRAPH_MODE_FIELD, GRAPH_ENABLE);
	SetScreen(Gs, Gs.Width, Gs.Height);
	graph_set_bgcolor(0, 0, 0);
	Gs.BackBuffer = 0;
	graph_set_framebuffer_filtered(Gs.Frames[1].FBP * 2048, Gs.Width, GraphPsm(ColorFormat), 0, 0);
	graph_enable_output();
	Gs.LastFlipField = Leon::PS2::FPS2VerticalBlank::GetFieldCount();
	Gs.bReady = true;
	Leon::PS2::AppendDrawEnvironment(Gs);
	// The first frame, a clear, on screen at once: WaitVSync kicks it, ShowPendingFrame shows it when the GS is done.
	ClearColor(0.125f, 0.3125f, 0.75f);
	WaitVSync();
	Leon::PS2::ShowPendingFrame(Gs);
	UE_LOG(LogRHI, Log, "FPS2RHI::InitDisplay: %dx%d, %s color, Z24, double buffered, %s (%.2f fields a second)",
		Gs.Width, Gs.Height, ColorFormat == EGSPixelFormat::PSMCT32 ? "32-bit" : "16-bit dithered",
		Gs.VideoMode == EGSVideoMode::Pal ? "PAL" : "NTSC", double(FGSFieldPacer::GetFieldsPerSecond(Gs.VideoMode)));
	return true;
}

void FPS2RHI::ShutdownDisplay()
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return;
	}
	Gs.bReady = false;
	Leon::PS2::DropPendingFrame(Gs);
	for (FGSCommandList& List : Gs.FrameLists)
	{
		List.Reset();
	}
	Leon::PS2::BeginNextFrame(Gs);
	Leon::PS2::FreeFrameChains(Gs);
	Leon::PS2::FPS2VerticalBlank::Remove();
	graph_shutdown();
}

void FPS2RHI::ClearColor(float R, float G, float B)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (Gs.bReady)
	{
		AppendClear(Gs, FGSDebugDraw::UnitColor(R, G, B));
	}
}

void FPS2RHI::Submit(const FGSCommandList& List)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return;
	}
	Gs.GetFrameList().Append(List);
	Leon::PS2::AppendDrawEnvironment(Gs);
}

void FPS2RHI::RetireInPlaceImages()
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady)
	{
		return;
	}
	Gs.GetFrameList().CopyInPlaceImages();
	Leon::PS2::WaitPendingFrameDma(Gs);
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
	// Frame N's chain, while the DMA and the GS may still be on frame N - 1.
	Leon::PS2::BuildFrameChain(Gs);
	// Frame N - 1 on screen once the GS has finished it, at the field the sync interval asks for.
	Leon::PS2::ShowPendingFrame(Gs);
	// Frame N to the GIF without waiting: the GS draws it while the EE makes frame N + 1 into the other list, chain
	// and frame buffer.
	Leon::PS2::KickFrameChain(Gs);
	Leon::PS2::BeginNextFrame(Gs);
	Gs.BackBuffer ^= 1;
	Gs.FrameIndex ^= 1;
	// Frame N - 1's list: ShowPendingFrame retired its chain, so nothing reads its images any more.
	Gs.GetFrameList().Reset();
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
