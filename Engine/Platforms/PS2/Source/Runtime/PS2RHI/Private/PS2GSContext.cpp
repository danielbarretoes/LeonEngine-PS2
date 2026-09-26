#include "PS2GSContext.h"

#include "GSGifPacket.h"
#include "Math/UnrealMathUtility.h"

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <string.h>

namespace Leon::PS2
{

	namespace
	{

		/** The most quadwords one normal-mode DMA transfer moves (D_QWC is 16 bits). */
		constexpr int32 MaxDmaQuadwords = 0xffff;

		[[nodiscard]] uint8 UnitToByte(float Value)
		{
			return uint8(FMath::Clamp(int32((Value * 255.0f) + 0.5f), 0, 255));
		}

		/** Grows the DMA buffer to NumQuadwords (packet_init aligns it for DMA). */
		[[nodiscard]] bool ReservePacket(FPS2GSContext& Gs, uint32 NumQuadwords)
		{
			if (Gs.Packet != nullptr && Gs.Packet->qwords >= NumQuadwords)
			{
				return true;
			}
			if (Gs.Packet != nullptr)
			{
				packet_free(Gs.Packet);
			}
			Gs.Packet = packet_init(int(FMath::Max(NumQuadwords, 4096u)), PACKET_NORMAL);
			return Gs.Packet != nullptr;
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
		GetDrawEnvironment(Gs).Append(Gs.FrameList);
	}

	void AppendDepthTest(FPS2GSContext& Gs, bool bDepthTest)
	{
		Gs.FrameList.SetTest(0, FGSDrawEnvironment::DepthTest(bDepthTest));
	}

	FGSXYZ ScreenVertex(float X, float Y, uint32 Z)
	{
		const FPS2GSContext& Gs = GetGSContext();
		return GetDrawEnvironment(Gs).PixelVertex(X + (float(Gs.Width) * 0.5f), Y + (float(Gs.Height) * 0.5f), Z);
	}

	FGSRGBAQ UnitColor(float R, float G, float B, uint8 A)
	{
		FGSRGBAQ Color;
		Color.R = UnitToByte(R);
		Color.G = UnitToByte(G);
		Color.B = UnitToByte(B);
		Color.A = A;
		return Color;
	}

	void FlushFrame(FPS2GSContext& Gs)
	{
		static TArray<uint64> Quadwords;
		Quadwords.Reset();
		FGSGifPacket::Build(Gs.FrameList, true, Quadwords);
		Gs.FrameList.Reset();
		const uint32 NumQuadwords = uint32(Quadwords.Num() / 2);
		if (NumQuadwords == 0 || !ReservePacket(Gs, NumQuadwords))
		{
			return;
		}
		Gs.PacketQuadwordsPeak = FMath::Max(Gs.PacketQuadwordsPeak, NumQuadwords);
		memcpy(Gs.Packet->data, Quadwords.GetData(), size_t(NumQuadwords) * 16);
		for (uint32 First = 0; First < NumQuadwords; First += MaxDmaQuadwords)
		{
			const int32 Count = int32(FMath::Min(NumQuadwords - First, uint32(MaxDmaQuadwords)));
			dma_channel_send_normal(DMA_CHANNEL_GIF, Gs.Packet->data + First, Count, 0, 0);
			dma_wait_fast();
		}
		draw_wait_finish();
	}

} // namespace Leon::PS2
