#include "PS2GSContext.h"

#include "DynamicRHI.h"
#include "GSGifPacket.h"
#include "HAL/LowLevelMemTracker.h"
#include "Math/UnrealMathUtility.h"
#include "PS2VU1Encoder.h"
#include "PS2VerticalBlank.h"
#include "RHIDeferredRelease.h"
#include "Stats/Stats.h"
#include "Templates/AlignmentTemplates.h"

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <kernel.h>

DECLARE_CYCLE_STAT(TEXT("Frame Chain"), STAT_PS2FrameChain, STATGROUP_RHI);
DECLARE_CYCLE_STAT(TEXT("VIF1 DMA"), STAT_PS2Vif1Dma, STATGROUP_RHI);
DECLARE_CYCLE_STAT(TEXT("GS Finish"), STAT_PS2GsFinish, STATGROUP_RHI);
DECLARE_CYCLE_STAT(TEXT("Vertical Blank Wait"), STAT_PS2VSyncWait, STATGROUP_RHI);

namespace Leon::PS2
{

	namespace
	{

		/** A chain buffer's alignment and size step: the UCAB's and the DMAC's 128-byte bursts, two cache lines. */
		constexpr uint32 ChainAlignment = 128;
		constexpr uint32 ChainLineQuadwords = ChainAlignment / 16;
		/** The first buffer of a chain: 64 KB (a frame of ShooterGame is about 25 KB). */
		constexpr uint32 MinChainQuadwords = 4096;

		void FreeChain(FPS2GifChain& Chain)
		{
			check(!Chain.bInFlight);
			if (Chain.Memory != nullptr)
			{
				FMemory::Free(Chain.Memory);
			}
			Chain = FPS2GifChain();
		}

		/** Grows Chain's buffer to NumQuadwords at least; false when the heap has no room. */
		[[nodiscard]] bool ReserveChain(FPS2GifChain& Chain, uint32 NumQuadwords)
		{
			if (Chain.CapacityQuadwords >= NumQuadwords)
			{
				return true;
			}
			FreeChain(Chain);
			LLM_SCOPE(ELLMTag::RenderLists);
			const uint32 Capacity = Align(FMath::Max(NumQuadwords, MinChainQuadwords), ChainLineQuadwords);
			Chain.Memory = static_cast<uint8*>(FMemory::Malloc(SIZE_T(Capacity) * 16, ChainAlignment));
			if (Chain.Memory == nullptr)
			{
				return false;
			}
			// No line of the buffer may stay in the data cache from the heap's earlier use: written back later, a dirty
			// one would overwrite what the UCAB wrote. The buffer owns its lines whole (aligned, whole lines).
			SyncDCache(Chain.Memory, Chain.Memory + (SIZE_T(Capacity) * 16) - 1);
			Chain.Quadwords = static_cast<uint64*>(UCAB_SEG(Chain.Memory));
			Chain.CapacityQuadwords = Capacity;
			return true;
		}

		/** The frame's vertex batches for VU1 (FPS2VU1). */
		FPS2VU1BatchEncoder GBatchEncoder;

		/** Waits for the pending frame's GS drawing (its FINISH) and DMA; its chain and list may then be reused. */
		void RetirePendingFrame(FPS2GSContext& Gs)
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_PS2GsFinish);
				// FINISH is the chain's last write: the GS raises it once it has drawn everything before it.
				draw_wait_finish();
			}
			{
				SCOPE_CYCLE_COUNTER(STAT_PS2Vif1Dma);
				dma_channel_wait(DMA_CHANNEL_VIF1, 0);
			}
			Gs.Chains[Gs.PendingChain].bInFlight = false;
			Gs.bFramePending = false;
			// Nothing reads the frame's memory any more: what was released while it was recorded goes.
			Gs.CompletedFrame = Gs.PendingFrame;
			FRHIDeferredRelease::SetFrameCounters(Gs.RecordingFrame, Gs.CompletedFrame);
		}

	} // namespace

	FPS2GSContext& GetGSContext()
	{
		static FPS2GSContext Context{};
		return Context;
	}

	int32 AllocateVram(int32 Width, int32 Height, int32 Psm, int32 Alignment)
	{
		const int32 Address = graph_vram_allocate(Width, Height, Psm, Alignment);
		if (Address >= 0)
		{
			const int32 End = Address + graph_vram_size(Width, Height, Psm, Alignment);
			FPS2GSContext& Gs = GetGSContext();
			Gs.VramEndWords = FMath::Max(Gs.VramEndWords, End);
		}
		return Address;
	}

	FGSDrawEnvironment GetDrawEnvironment(const FPS2GSContext& Gs)
	{
		FGSDrawEnvironment Environment;
		Environment.Frame = Gs.Frames[Gs.BackBuffer];
		Environment.ZBuf = Gs.ZBuf;
		Environment.Width = uint16(Gs.Width);
		Environment.Height = uint16(Gs.Height);
		return Environment;
	}

	void AppendDrawEnvironment(FPS2GSContext& Gs)
	{
		GetDrawEnvironment(Gs).Append(Gs.GetFrameList());
	}

	void BuildFrameChain(FPS2GSContext& Gs)
	{
		SCOPE_CYCLE_COUNTER(STAT_PS2FrameChain);
		const FGSCommandList& List = Gs.GetFrameList();
		FPS2GifChain& Chain = Gs.Chains[Gs.FrameIndex];
		// Its last use was two frames ago, retired when the frame after it was kicked.
		check(!Chain.bInFlight);
		Chain.NumQuadwords = 0;
		// The vertex batches go to VU1 (the renderer records them only when FPS2VU1::IsEnabled).
		FPS2VU1BatchEncoder* Encoder = nullptr;
		if (List.GetVertexBatches().Num() > 0)
		{
			GBatchEncoder.Begin(GetDrawEnvironment(Gs), true);
			Encoder = &GBatchEncoder;
		}
		const uint32 Capacity = FGSGifPacket::GetChainCapacity(List, true, Encoder);
		if (!ReserveChain(Chain, Capacity))
		{
			UE_LOG(LogRHI, Error, "FPS2RHI: no memory for a frame's DMA chain of %u quadwords; the frame is dropped",
				Capacity);
			return;
		}
		Chain.NumQuadwords = FGSGifPacket::BuildChain(List, true, Chain.Quadwords, Chain.CapacityQuadwords, Encoder);
	}

	void ShowPendingFrame(FPS2GSContext& Gs)
	{
		if (!Gs.bFramePending)
		{
			return;
		}
		const int32 Shown = Gs.PendingBuffer;
		RetirePendingFrame(Gs);
		{
			// Asleep on the vertical blank's semaphore until the field the pacer picks: SyncInterval fields after the
			// last flip, or the next blank for a late frame.
			SCOPE_CYCLE_COUNTER(STAT_PS2VSyncWait);
			const uint32 FlipField = FGSFieldPacer::GetFlipField(
				Gs.LastFlipField, FPS2VerticalBlank::GetFieldCount(), uint32(Gs.SyncInterval));
			Gs.LastFlipField = FPS2VerticalBlank::WaitForField(FlipField);
		}
		const FGSFrame& Drawn = Gs.Frames[Shown];
		graph_set_framebuffer_filtered(Drawn.FBP * 2048, Gs.Width, int(Drawn.PSM), 0, 0);
	}

	void KickFrameChain(FPS2GSContext& Gs)
	{
		FPS2GifChain& Chain = Gs.Chains[Gs.FrameIndex];
		if (Chain.NumQuadwords == 0)
		{
			return;
		}
		SCOPE_CYCLE_COUNTER(STAT_PS2Vif1Dma);
		// What the chain REFs was written through the data cache (the uploads' pixels in the list's copy, a cooked
		// texture's levels as it was loaded, the meshes' streams): the whole cache (8 KB) is written back. Then the
		// UCAB's last writes reach memory before the DMAC reads the chain, and the transfer runs on its own, its tags'
		// VIFcodes to VIF1 (TTE).
		FlushCache(WRITEBACK_DCACHE);
		EE_SYNCL();
		dma_channel_send_chain_ucab(DMA_CHANNEL_VIF1, Chain.Quadwords, int(Chain.NumQuadwords), DMA_FLAG_TRANSFERTAG);
		Chain.bInFlight = true;
		Gs.bFramePending = true;
		Gs.PendingChain = Gs.FrameIndex;
		Gs.PendingBuffer = Gs.BackBuffer;
		Gs.PendingFrame = Gs.RecordingFrame;
	}

	void BeginNextFrame(FPS2GSContext& Gs)
	{
		// A frame not kicked (dropped, or empty) reads nothing: it is complete at once.
		if (!Gs.bFramePending || Gs.PendingFrame != Gs.RecordingFrame)
		{
			Gs.CompletedFrame = Gs.RecordingFrame;
		}
		++Gs.RecordingFrame;
		FRHIDeferredRelease::SetFrameCounters(Gs.RecordingFrame, Gs.CompletedFrame);
	}

	void WaitPendingFrameDma(FPS2GSContext& Gs)
	{
		if (Gs.bFramePending)
		{
			SCOPE_CYCLE_COUNTER(STAT_PS2Vif1Dma);
			dma_channel_wait(DMA_CHANNEL_VIF1, 0);
		}
	}

	void DropPendingFrame(FPS2GSContext& Gs)
	{
		if (Gs.bFramePending)
		{
			RetirePendingFrame(Gs);
		}
	}

	void FreeFrameChains(FPS2GSContext& Gs)
	{
		for (FPS2GifChain& Chain : Gs.Chains)
		{
			FreeChain(Chain);
		}
	}

} // namespace Leon::PS2
