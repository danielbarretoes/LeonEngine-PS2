#pragma once

// Private GS state for the PS2 RHI plugin (not a public Engine header).

#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSFieldPacer.h"
#include "GSTypes.h"

namespace Leon::PS2
{

	/**
	 * A frame's DMA chain for the VIF1 channel (FGSGifPacket::BuildChain: the GS writes by DIRECT, the vertex batches
	 * for VU1): a buffer of whole 128-byte lines, 128-byte aligned, that the EE writes only through the uncached
	 * accelerated segment, so the data cache never holds it and nothing has to be written back before the DMA reads it.
	 */
	struct FPS2GifChain
	{
		/** The allocation (FMemory, its cached address: never read or written through it). */
		uint8* Memory = nullptr;
		/** Memory through the uncached accelerated segment (0x30000000), where the chain is written. */
		uint64* Quadwords = nullptr;
		uint32 CapacityQuadwords = 0;
		/** The chain built this frame (0: nothing to send). */
		uint32 NumQuadwords = 0;
		/** Kicked and not yet retired: the DMA or the GS may still read it, or the images it refers to. */
		bool bInFlight = false;
	};

	/**
	 * The display and the frames. Every draw appends to the recording frame's list; WaitVSync builds its chain, shows
	 * the frame before it once the GS has finished it, and kicks the chain: the DMA and the GS draw frame N while the
	 * EE makes frame N + 1 into the other list and buffer.
	 */
	struct FPS2GSContext
	{
		int32 Width = 0;
		int32 Height = 0;
		/** Double buffered: the GS draws into Frames[BackBuffer] while the CRTC shows the other one. */
		FGSFrame Frames[2];
		int32 BackBuffer = 0;
		FGSZBuf ZBuf;
		/**
		 * Two frames' GS work: FrameLists[FrameIndex] records while the other one's chain may still be in flight (its
		 * REF tags read the list's image data), and Chains[FrameIndex] is the recording frame's DMA buffer.
		 */
		FGSCommandList FrameLists[2];
		FPS2GifChain Chains[2];
		int32 FrameIndex = 0;
		/** A frame kicked and not shown yet: its chain and the frame buffer the GS draws it into. */
		bool bFramePending = false;
		int32 PendingChain = 0;
		/**
		 * The frames' numbers (FRHIDeferredRelease): the one being recorded, the pending one and the last one the DMA
		 * and the GS have finished with. Memory released while frame N is recorded is freed once N has completed.
		 */
		uint64 RecordingFrame = 1;
		uint64 PendingFrame = 0;
		uint64 CompletedFrame = 0;
		int32 PendingBuffer = 0;
		bool bReady = false;
		/** End of the libgraph bump allocator (32-bit words): the VRAM in use, for GetGPUMemoryStats. */
		int32 VramEndWords = 0;
		/** The television mode InitDisplay chose (the console's region, or -PAL / -NTSC). */
		EGSVideoMode VideoMode = EGSVideoMode::Ntsc;
		/** FPS2RHI::SetSyncInterval, and the field the last frame was shown on (FPS2VerticalBlank's count). */
		int32 SyncInterval = 1;
		uint32 LastFlipField = 0;

		/** The recording frame's list. */
		[[nodiscard]] FGSCommandList& GetFrameList()
		{
			return FrameLists[FrameIndex];
		}
	};

	[[nodiscard]] FPS2GSContext& GetGSContext();

	/** The drawing environment of the frame being drawn (the back buffer). */
	[[nodiscard]] FGSDrawEnvironment GetDrawEnvironment(const FPS2GSContext& Gs);

	/** graph_vram_allocate plus the VRAM bookkeeping: a word address, or < 0 when the VRAM is full. */
	[[nodiscard]] int32 AllocateVram(int32 Width, int32 Height, int32 Psm, int32 Alignment);

	/** Appends the drawing environment (FGSDrawEnvironment, on the back buffer) to the frame list. */
	void AppendDrawEnvironment(FPS2GSContext& Gs);

	/**
	 * Builds the recording frame's list into its chain (the GIF Packet stat): the tags and register writes through the
	 * uncached accelerated segment, the uploads' pixels by REF, written back from the data cache.
	 */
	void BuildFrameChain(FPS2GSContext& Gs);

	/**
	 * Shows the pending frame: waits for the GS to finish it (CSR.FINISH, the GS Finish stat), retires its chain (the
	 * GIF DMA stat), sleeps until the field FGSFieldPacer picks (the Vertical Blank Wait stat) and flips the CRTC to
	 * it. Nothing without a pending frame.
	 */
	void ShowPendingFrame(FPS2GSContext& Gs);

	/** Starts the recording frame's chain on the VIF1 channel and returns at once; it becomes the pending frame. */
	void KickFrameChain(FPS2GSContext& Gs);

	/**
	 * Waits until the pending frame's DMA has read its chain and the images it refers to; the frame stays pending (it
	 * is still shown). Nothing without a pending frame.
	 */
	void WaitPendingFrameDma(FPS2GSContext& Gs);

	/** Waits for the pending frame's DMA and drawing without showing it (before the GS is reset or shut down). */
	void DropPendingFrame(FPS2GSContext& Gs);

	/**
	 * Starts the next frame's number once the recording frame is kicked or dropped (its list reset): what the renderer
	 * releases from now on waits for the next frame (FRHIDeferredRelease).
	 */
	void BeginNextFrame(FPS2GSContext& Gs);

	/** Frees both chains' buffers (no chain may be in flight). */
	void FreeFrameChains(FPS2GSContext& Gs);

} // namespace Leon::PS2
