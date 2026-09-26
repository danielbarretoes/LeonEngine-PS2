#pragma once

// Private GS state for the PS2 RHI plugin (not a public Engine header).

#include "GSCommandList.h"
#include "GSTypes.h"

#include <packet.h>

namespace Leon::PS2
{

	/**
	 * The display and the frame's command list. Every draw appends to FrameList; FlushFrame sends it to the GIF as one
	 * PATH3 packet (FGSGifPacket) and waits for the GS to finish.
	 */
	struct FPS2GSContext
	{
		int32 Width = 0;
		int32 Height = 0;
		/** Double buffered: the GS draws into Frames[BackBuffer] while the CRTC shows the other one. */
		FGSFrame Frames[2];
		int32 BackBuffer = 0;
		FGSZBuf ZBuf;
		/** The frame's GS work, sent by FlushFrame. */
		FGSCommandList FrameList;
		/** The DMA buffer the packet is copied into (16-byte aligned, grown on demand). */
		packet_t* Packet = nullptr;
		bool bReady = false;
		/** End of the libgraph bump allocator (32-bit words): the VRAM in use, for GetGPUMemoryStats. */
		int32 VramEndWords = 0;
		/** The largest frame packet so far, in quadwords. */
		uint32 PacketQuadwordsPeak = 0;
	};

	[[nodiscard]] FPS2GSContext& GetGSContext();

	/** graph_vram_allocate plus the VRAM bookkeeping: a word address, or < 0 when the VRAM is full. */
	[[nodiscard]] int32 AllocateVram(int32 Width, int32 Height, int32 Psm, int32 Alignment);

	/**
	 * Appends the drawing environment to the frame list: the back buffer, the Z buffer, the window offset that centers
	 * screen coordinates, the whole screen as scissor, the depth test GEQUAL, standard blending, and dithering on a
	 * 16-bit frame buffer.
	 */
	void AppendDrawEnvironment(FPS2GSContext& Gs);

	/** Appends a TEST_1 write: the 3D depth test (GEQUAL) or none (ALWAYS, for 2D overlays). */
	void AppendDepthTest(FPS2GSContext& Gs, bool bDepthTest);

	/** A vertex at screen coordinates (pixels, origin at the screen's center) with depth Z. */
	[[nodiscard]] FGSXYZ ScreenVertex(float X, float Y, uint32 Z = 0);

	/** A color in [0, 1] as the GS's RGBAQ, with 1.0 at 0xff (untextured) and alpha 0x80 (1.0). */
	[[nodiscard]] FGSRGBAQ UnitColor(float R, float G, float B, uint8 A = 0x80);

	/** Sends the frame list to the GIF and waits for the GS to finish; the list is empty afterwards. */
	void FlushFrame(FPS2GSContext& Gs);

} // namespace Leon::PS2
